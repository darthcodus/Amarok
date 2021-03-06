include_directories(
  ${AMAROK_TEST_TREE}
  ${AMAROK_SOURCE_TREE}
  ${AMAROK_SOURCE_TREE}/core-impl/collections/support
  ${AMAROK_UTILITY_TREE}
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_BINARY_DIR}/tests
  ${AMAROK_COLLECTION_SUPPORT_DIR}
  ${KDE4_INCLUDE_DIR}
  ${QT_INCLUDES}
  ${GOOGLEMOCK_INCLUDE_DIR}
  )

#------------------------------- TestMemoryQueryMaker -------------------------------

set( testmemoryquerymaker_SRCS
        TestMemoryQueryMaker.cpp
    )

kde4_add_unit_test( testmemoryquerymaker ${testmemoryquerymaker_SRCS} )

if(APPLE)
    SET_TARGET_PROPERTIES(testmemoryquerymaker PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(APPLE)
add_dependencies( testmemoryquerymaker amarokcore )
add_dependencies( testmemoryquerymaker amaroklib )
target_link_libraries(testmemoryquerymaker
    amarokcore
    amaroklib
    ${KDE4_KDECORE_LIBS}
    ${QT_QTGUI_LIBRARY}
    ${KDE4_THREADWEAVER_LIBRARIES}
    ${QT_QTTEST_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${GOOGLEMOCK_LIBRARIES})

#-------------------------------- Test ArtistHelper -----------------------

set( testartisthelper_SRCS
        TestArtistHelper.cpp
        ${AMAROK_SOURCE_TREE}/core-impl/collections/support/ArtistHelper.cpp
    )


kde4_add_unit_test( testartisthelper ${testartisthelper_SRCS} )

    if(APPLE)
        SET_TARGET_PROPERTIES(testartisthelper PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif(APPLE)

target_link_libraries( testartisthelper
                        ${QT_QTTEST_LIBRARY}
                        ${QT_QTCORE_LIBRARY}
                        ${KDE4_KDECORE_LIBS})

