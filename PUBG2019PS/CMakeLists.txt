set(PROJECT_NAME PUBG2019PS)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "framework.h"
    "MinHook.h"
    "pch.h"
    "UnrealContainers.hpp"
    "UtfN.hpp"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "CoreUObject_functions.cpp"
    "dllmain.cpp"
    "pch.cpp"
    "SDK.hpp"
    "SDK/Basic.cpp"
    "SDK/Basic.hpp"
    "SDK/CoreUObject_classes.hpp"
    #"SDK/CoreUObject_parameters.hpp"
    "SDK/CoreUObject_structs.hpp"
    "SDK/Engine_classes.hpp"
    "SDK/Engine_functions.cpp"
    "SDK/Engine_parameters.hpp"
    "SDK/Engine_structs.hpp"
    "SDK/GameplayTags_classes.hpp"
    #"SDK/GameplayTags_parameters.hpp"
    "SDK/GameplayTags_structs.hpp"
    "SDK/GameplayTasks_classes.hpp"
    "SDK/GameplayTasks_functions.cpp"
    "SDK/GameplayTasks_parameters.hpp"
    "SDK/GameplayTasks_structs.hpp"
    "SDK/Landscape_classes.hpp"
    "SDK/Landscape_functions.cpp"
    "SDK/Landscape_parameters.hpp"
    "SDK/Landscape_structs.hpp"
    "SDK/TslGame_classes.hpp"
    "SDK/TslGame_functions.cpp"
    "SDK/TslGame_parameters.hpp"
    "SDK/TslGame_structs.hpp"
    "UtilsDefs.h"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

set(ALL_FILES_WITH_PATH)
foreach(file ${ALL_FILES})
    list(APPEND ALL_FILES_WITH_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
endforeach()

################################################################################
# Target
################################################################################
#add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
add_library(${PROJECT_NAME} SHARED ${ALL_FILES_WITH_PATH})   

target_precompile_headers(${PROJECT_NAME} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
)

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE PUBG2019PS)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
endif()
################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "PUBG2019PS_EXPORTS;"
        "_WINDOWS;"
        "_USRDLL;"
        "UNICODE;"
        "_UNICODE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "WIN32;"
        "PUBG2019PS_EXPORTS;"
        "_WINDOWS;"
        "_USRDLL;"
        "UNICODE;"
        "_UNICODE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /std:c++latest;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING};
            /Y-
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            /DEBUG;
            /SUBSYSTEM:WINDOWS
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            /DEBUG;
            /SUBSYSTEM:WINDOWS
        )
    endif()
endif()

