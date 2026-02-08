# ============================================================================
# AI Module
# AI Backend integration for natural language drone control
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        AIChatController.cc
        AIChatController.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------------------------------------------------------
# QML Module for AI Chat UI
# ----------------------------------------------------------------------------
qt_add_library(AIModule STATIC)

qt_add_qml_module(AIModule
    URI QGroundControl.AI
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        AIChatPanel.qml
    NO_PLUGIN
)
