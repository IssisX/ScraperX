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
