"""Money: the caps in work/budget.yml and the spend in the append-only runs.log.

All amounts are decimal US dollars (never binary floats). Days and months are UTC. Every paid call
writes a `reserve` line with its worst-case cost before it runs and an `end` line with its actual
cost after; a reserve with no end counts in full, so a crash between the two never hides spend.
Refusals are logged too (`refused`, $0) so the digest can say what the budget paused.
"""

from __future__ import annotations

import dataclasses
import datetime as dt
import json
from decimal import ROUND_CEILING, Decimal, InvalidOperation
from typing import Iterable, Mapping

MONTH_CAP_CEILING = Decimal("100")   # the owner's API credit; budget.yml may not exceed it
MICRO = Decimal("0.000001")
PHASES = ("reserve", "end", "refused")


class BudgetConfigError(ValueError):
    pass


def usd(text: object, where: str) -> Decimal:
    """A non-negative dollar amount from config or the log; raises naming where it came from."""
    try:
        d = Decimal(str(text))
    except (InvalidOperation, ValueError) as err:
        raise BudgetConfigError(f"{where}: {text!r} is not a dollar amount") from err
    if not d.is_finite() or d < 0:
        raise BudgetConfigError(f"{where}: {text!r} must be a finite amount >= 0")
    return d


@dataclasses.dataclass(frozen=True)
class Price:
    input_per_mtok: Decimal
    output_per_mtok: Decimal

    def cost(self, input_tokens: int, output_tokens: int) -> Decimal:
        raw = (Decimal(input_tokens) * self.input_per_mtok + Decimal(output_tokens) * self.output_per_mtok) / Decimal(1_000_000)
        return raw.quantize(MICRO, rounding=ROUND_CEILING)


@dataclasses.dataclass(frozen=True)
class CallSpec:
    model: str
    max_tokens: int
    effort: str
    max_input_tokens: int


@dataclasses.dataclass(frozen=True)
class Budget:
    day: Decimal
    month: Decimal
    job: Decimal
    prices: Mapping[str, Price]
    calls: Mapping[str, CallSpec]
    orchestrator_enabled: bool
    orchestrator_agent_id: str
    orchestrator_environment_id: str
    orchestrator_session_usd: Decimal
    max_log_bytes: int
    orchestrator_vault_id: str = ""

    def price(self, model: str) -> Price:
        if model not in self.prices:
            raise BudgetConfigError(f"budget.yml: no price for model {model!r}; the switch refuses unpriced calls")
        return self.prices[model]


def budget_from_config(cfg: Mapping) -> Budget:
    caps = cfg.get("caps") or {}
    month = usd(caps.get("month_usd"), "caps.month_usd")
    if month > MONTH_CAP_CEILING:
        raise BudgetConfigError(f"caps.month_usd {month} is above the ${MONTH_CAP_CEILING} API credit")
    day = usd(caps.get("day_usd"), "caps.day_usd")
    job = usd(caps.get("job_usd"), "caps.job_usd")
    if day > month or job > month:
        raise BudgetConfigError("caps: day_usd and job_usd must not exceed month_usd")
    prices = {}
    for model, p in (cfg.get("prices_per_mtok") or {}).items():
        prices[model] = Price(usd(p.get("input"), f"prices_per_mtok.{model}.input"),
                              usd(p.get("output"), f"prices_per_mtok.{model}.output"))
    calls = {}
    for name, c in (cfg.get("calls") or {}).items():
        try:
            spec = CallSpec(str(c["model"]), int(c["max_tokens"]), str(c["effort"]), int(c["max_input_tokens"]))
        except (KeyError, TypeError, ValueError) as err:
            raise BudgetConfigError(f"calls.{name}: needs model, max_tokens, effort, max_input_tokens") from err
        if spec.model not in prices:
            raise BudgetConfigError(f"calls.{name}: model {spec.model!r} has no price")
        if spec.max_tokens <= 0 or spec.max_input_tokens <= 0:
            raise BudgetConfigError(f"calls.{name}: token limits must be positive")
        calls[name] = spec
    orch = cfg.get("orchestrator") or {}
    session_usd = usd(orch.get("session_usd", "0"), "orchestrator.session_usd")
    if session_usd > job:
        raise BudgetConfigError("orchestrator.session_usd must not exceed caps.job_usd")
    return Budget(day, month, job, prices, calls, bool(orch.get("enabled", False)),
                  str(orch.get("agent_id") or ""), str(orch.get("environment_id") or ""), session_usd,
                  int(cfg.get("max_log_bytes", 900_000)), str(orch.get("vault_id") or ""))


@dataclasses.dataclass(frozen=True)
class Entry:
    ts: dt.datetime
    run: str
    phase: str
    kind: str
    job: str
    usd: Decimal
    note: str


def parse_log(text: str) -> list[Entry]:
    """runs.log -> entries. A malformed line raises: the switch refuses to spend on a log it cannot read."""
    out = []
    for n, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        try:
            o = json.loads(line)
            ts = dt.datetime.fromisoformat(str(o["ts"]).replace("Z", "+00:00"))
            if ts.tzinfo is None:
                raise ValueError("timestamp without a zone")
            phase = str(o["phase"])
            if phase not in PHASES:
                raise ValueError(f"unknown phase {phase!r}")
            out.append(Entry(ts.astimezone(dt.timezone.utc), str(o["run"]), phase, str(o.get("kind", "")),
                             str(o.get("job", "")), usd(o.get("usd", "0"), f"runs.log line {n}"), str(o.get("note", ""))))
        except (KeyError, ValueError, TypeError) as err:
            raise BudgetConfigError(f"runs.log line {n} is not a run record: {err}") from err
    return out


def spend_by_run(entries: Iterable[Entry]) -> dict[str, tuple[Entry, Decimal]]:
    """Each run's first entry and its spend: the end amount if it ended, else the reserve."""
    first: dict[str, Entry] = {}
    reserve: dict[str, Decimal] = {}
    end: dict[str, Decimal] = {}
    for e in entries:
        first.setdefault(e.run, e)
        if e.phase == "reserve":
            reserve[e.run] = reserve.get(e.run, Decimal(0)) + e.usd
        elif e.phase == "end":
            end[e.run] = e.usd
    return {r: (first[r], end.get(r, reserve.get(r, Decimal(0)))) for r in first}


@dataclasses.dataclass(frozen=True)
class Spent:
    day: Decimal
    month: Decimal
    job: Decimal


def spent(entries: Iterable[Entry], now: dt.datetime, job: str) -> Spent:
    now = now.astimezone(dt.timezone.utc)
    day = month = on_job = Decimal(0)
    for first, amount in spend_by_run(entries).values():
        t = first.ts
        if (t.year, t.month) == (now.year, now.month):
            month += amount
            if t.date() == now.date():
                day += amount
        if job and first.job == job:
            on_job += amount
    return Spent(day, month, on_job)


@dataclasses.dataclass(frozen=True)
class Check:
    ok: bool
    reason: str


def check(budget: Budget, entries: Iterable[Entry], now: dt.datetime, job: str, cost: Decimal,
          job_cap_override: str = "") -> Check:
    """May a call that can cost at most `cost` run now? Refuses at any cap; never approximates."""
    s = spent(entries, now, job)
    job_cap = budget.job
    if job_cap_override:
        job_cap = min(usd(job_cap_override, "budget comment"), budget.month)
    for name, used, cap in (("month", s.month, budget.month), ("day", s.day, budget.day),
                            ("job " + (job or "-"), s.job, job_cap)):
        if used + cost > cap:
            return Check(False, f"{name} cap ${cap:.2f}: spent ${used:.2f}, this call up to ${cost:.2f}")
    return Check(True, f"month ${s.month:.2f}/{budget.month:.2f}, day ${s.day:.2f}/{budget.day:.2f}, "
                       f"job ${s.job:.2f}/{job_cap:.2f}, this call up to ${cost:.2f}")


def worst_case(budget: Budget, call: str, input_tokens: int) -> Decimal:
    spec = budget.calls[call]
    if input_tokens > spec.max_input_tokens:
        raise BudgetConfigError(f"{call}: {input_tokens} input tokens is above max_input_tokens {spec.max_input_tokens}")
    return budget.price(spec.model).cost(input_tokens, spec.max_tokens)


def record(ts: dt.datetime, run: str, phase: str, kind: str, job: str, amount: Decimal, note: str = "") -> str:
    """One runs.log line (JSON, sorted keys, no secrets: notes carry ids and counts only)."""
    if phase not in PHASES:
        raise ValueError(f"unknown phase {phase!r}")
    return json.dumps({"ts": ts.astimezone(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "run": run,
                       "phase": phase, "kind": kind, "job": job, "usd": str(amount), "note": note[:200]},
                      sort_keys=True)
