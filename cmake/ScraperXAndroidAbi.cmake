# Android ABI names differ from Godot's GDExtension feature tags.
# Never place an emulator library over the shipping ARM64 library.
function(scraperx_android_extension_directory abi output)
    if(abi STREQUAL "arm64-v8a")
        set(${output} "arm64" PARENT_SCOPE)
    elseif(abi STREQUAL "x86_64")
        set(${output} "x86_64" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unsupported ScraperX Android ABI '${abi}'; expected arm64-v8a or x86_64")
    endif()
endfunction()

function(scraperx_configure_android_cpu abi)
    if(abi STREQUAL "x86_64")
        # Android's x86_64 ABI guarantees SSE4.2/POPCNT, not AVX, FMA or BMI.
        # Jolt has compile-time dispatch; its desktop defaults would silently
        # raise our Android CPU requirement above the advertised ABI.
        foreach(feature AVX AVX2 AVX512 LZCNT TZCNT F16C FMADD)
            set(USE_${feature} OFF CACHE BOOL "Android x86_64 ABI baseline" FORCE)
        endforeach()
    endif()
endfunction()
