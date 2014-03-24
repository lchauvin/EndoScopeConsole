set(extProjName "ArUco")

set(ARUCO_GIT_REPO "${git_protocol}://github.com/lchauvin/ArUco.git")
set(ARUCO_GIT_TAG "3763fd7fe0e740503f351d8e758c98e8c846fe08")

if(NOT DEFINED ArUco_DIR)
  set(ArUco_DEPEND ArUco)
  set(proj ArUco)

  ExternalProject_add(${proj}
    SOURCE_DIR ${proj}
    BINARY_DIR ${proj}-build
    
    GIT_REPOSITORY ${ARUCO_GIT_REPO}
    GIT_TAG ${ARUCO_GIT_TAG}
    CMAKE_ARGS
      ${CMAKE_OSX_EXTERNAL_PROJECT_ARGS}
      ${COMMON_EXTERNAL_PROJECT_ARGS}
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/${proj}-build
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    )
  set(ArUco_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
endif(NOT DEFINED ArUco_DIR)
