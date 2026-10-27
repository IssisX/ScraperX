# Every force on the Matron comes from her one per-tick Budget (issue #3).
# Fails if any of her source sets her velocity, adds an impulse, or adds a
# velocity step on her body outside that path. Placing her at rest (spawn,
# a test's placement) is not a force and is allowed.
file(GLOB her_sources "${SOURCE_DIR}/src/sim/matron*.cpp")
set(found 0)
foreach(source IN LISTS her_sources)
    file(STRINGS "${source}" lines)
    set(n 0)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        set(bad FALSE)
        if(line MATCHES "SetLinearVelocity\\(body_" OR line MATCHES "AddLinearVelocity\\(body_" OR
           line MATCHES "AddImpulse\\(")
            set(bad TRUE)
        elseif(line MATCHES "SetLinearAndAngularVelocity\\(body_" AND
               NOT line MATCHES "SetLinearAndAngularVelocity\\(body_, Vec3::sZero\\(\\), Vec3::sZero\\(\\)\\)")
            set(bad TRUE)
        elseif(line MATCHES "^[^/]*[^a-z_.]vy *= *[^=]" AND NOT line MATCHES "(float|double|auto) +vy *=")
            set(bad TRUE)
        endif()
        if(bad)
            message(STATUS "FAIL ${source}:${n}: ${line}")
            math(EXPR found "${found} + 1")
        endif()
    endforeach()
endforeach()
if(found GREATER 0)
    message(FATAL_ERROR "FAIL scraperx_sim matron one budget: ${found} velocity or impulse write(s) on her outside her Budget")
endif()
message(STATUS "PASS scraperx_sim matron one budget: no velocity or impulse write on her outside her Budget")
