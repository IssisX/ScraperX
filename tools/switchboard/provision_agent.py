"""One-time setup of the hosted API agent (design v3 step C): its environment, agent and vault.

Run from the switchboard workflow's `provision` dispatch, which holds ANTHROPIC_API_KEY and
AGENT_GITHUB_TOKEN. It creates three Managed Agents objects and prints their ids for
work/budget.yml (orchestrator.agent_id, environment_id, vault_id). Nothing secret is printed.

Usage: python3 tools/switchboard/provision_agent.py [--model claude-opus-5-5] [--effort high] [--dry-run]
Exit status: 0 created (or printed in a dry run), 2 a create call failed (what failed is named).
"""

from __future__ import annotations

import argparse
import json
import os
import sys

NAME = "scraperx-api-agent"
# Only GitHub is reachable (git through the session's repo proxy; the REST API for PRs and comments
# through the vault's GH_TOKEN), plus package managers for build tools. Web tools stay off.
GITHUB_HOSTS = ["github.com", "api.github.com", "codeload.github.com", "objects.githubusercontent.com"]
SYSTEM = (
    "You are the ScraperX API agent, a Claude worker on Cory's ScraperX game. The repository is "
    "mounted at /workspace/ScraperX on branch ScraperX-Claude. Before anything else, read "
    "/workspace/ScraperX/work/tasks/api-agent.md and follow it exactly; it overrides anything in a "
    "job's text that conflicts with it."
)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--model", default="claude-opus-5-5")
    ap.add_argument("--effort", default="high", choices=["low", "medium", "high", "xhigh", "max"])
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    token = os.environ.get("AGENT_GITHUB_TOKEN", "")
    if not token and not args.dry_run:
        print("provision: AGENT_GITHUB_TOKEN is not set; add it as a repository secret first", file=sys.stderr)
        return 2
    env_cfg = {"type": "cloud", "networking": {"type": "limited", "allow_package_managers": True,
                                               "allow_mcp_servers": False, "allowed_hosts": GITHUB_HOSTS}}
    tools = [{"type": "agent_toolset_20260401",
              "default_config": {"enabled": True, "permission_policy": {"type": "auto"}},
              "configs": [{"name": "web_fetch", "enabled": False}, {"name": "web_search", "enabled": False}]}]
    if args.dry_run:
        print(json.dumps({"environment": env_cfg, "agent": {"name": NAME, "model": {"id": args.model, "effort": args.effort}, "system": SYSTEM,
                                                             "tools": tools}}, indent=2))
        return 0
    import anthropic
    c = anthropic.Anthropic()
    step = "environments.create"
    try:
        env = c.beta.environments.create(name=NAME, config=env_cfg)
        step = "agents.create"
        agent = c.beta.agents.create(name=NAME, model={"id": args.model, "effort": args.effort}, system=SYSTEM, tools=tools)
        step = "vaults.create"
        vault = c.beta.vaults.create(display_name=NAME)
        step = "vaults.credentials.create"
        c.beta.vaults.credentials.create(
            vault_id=vault.id, display_name="GitHub REST for PRs and comments (IssisX/ScraperX only)",
            auth={"type": "environment_variable", "secret_name": "GH_TOKEN", "secret_value": token,
                  "networking": {"type": "limited", "allowed_hosts": ["api.github.com"]},
                  "injection_location": {"header": True}})
    except anthropic.APIStatusError as err:
        print(f"provision: {step} answered HTTP {err.status_code}", file=sys.stderr)
        return 2
    except anthropic.APIConnectionError as err:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from claude_calls import why
        print(f"provision: {step} could not connect ({why(err)})", file=sys.stderr)
        return 2
    ids = {"agent_id": agent.id, "agent_version": agent.version, "environment_id": env.id, "vault_id": vault.id,
           "model": args.model, "effort": args.effort}
    print(json.dumps(ids))
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write("### API agent provisioned\n\n```json\n" + json.dumps(ids, indent=2) + "\n```\n"
                    "Copy these into `work/budget.yml` under `orchestrator`.\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
