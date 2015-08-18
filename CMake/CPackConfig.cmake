# Info: http://www.itk.org/Wiki/CMake:Component_Install_With_CPack

set(CPACK_PACKAGE_CONTACT "Ahmet Bilgili <ahmet.bilgili@epfl.ch>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_LICENSE "MIT")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "bison, flex, qtbase5-dev, libqt5opengl5-dev, zlib1g-dev")

include(CommonCPack)
