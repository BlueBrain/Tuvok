# Copyright (c) 2013 ahmet.bilgili@epfl.ch

# Info: http://www.itk.org/Wiki/CMake:Component_Install_With_CPack

set(CPACK_PACKAGE_VENDOR "bluebrain.epfl.ch")
set(CPACK_PACKAGE_CONTACT "Ahmet Bilgili <ahmet.bilgili@epfl.ch>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tuvok ( Large scale, out-of-core data access")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6,
                                  libqt4-core,
                                  libqt4-gui,
                                  libqt4-opengl")

include(CommonCPack)
