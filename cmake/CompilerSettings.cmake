# Sanitizer flags are global: every linked object, GoogleTest included, must agree on them.
if(MCPRICER_SANITIZER)
  if(MSVC)
    if(NOT MCPRICER_SANITIZER STREQUAL "address")
      message(FATAL_ERROR "MSVC supports only MCPRICER_SANITIZER=address")
    endif()
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO /DEBUG)
  else()
    add_compile_options(-fsanitize=${MCPRICER_SANITIZER} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${MCPRICER_SANITIZER})
  endif()
endif()

# Link the MinGW runtime statically. Git Bash puts its own runtime directory first on PATH,
# so a dynamically linked program could load a DLL that does not match the compiler.
if(MINGW)
  add_link_options(-static)
endif()

# Applied per target so that third-party code is not held to our warning level.
function(mcpricer_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(MCPRICER_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(MCPRICER_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
