include_directories(
    ${KDE4_INCLUDE_DIR}
    ${QT_INCLUDES}
)

set( amarok_importer-fastforward_PART_SRCS
    FastForwardConfigWidget.cpp
    FastForwardManager.cpp
    FastForwardProvider.cpp
    FastForwardTrack.cpp
)

kde4_add_ui_files( amarok_importer-fastforward_PART_SRCS FastForwardConfigWidget.ui )

kde4_add_plugin( amarok_importer-fastforward ${amarok_importer-fastforward_PART_SRCS} )

target_link_libraries( amarok_importer-fastforward
    amarokcore
    amaroklib
    ${KDE4_KIO_LIBS}
    ${QT_QTSQL_LIBRARY}
)

install( TARGETS amarok_importer-fastforward DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES amarok_importer-fastforward.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
