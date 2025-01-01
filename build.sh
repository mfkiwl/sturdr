# Reset
Reset='\033[0m'           # Text Reset
Red='\033[0;31m'          # Red
Green='\033[0;32m'        # Green
Yellow='\033[0;33m'       # Yellow
Blue='\033[0;34m'         # Blue
Magenta='\033[0;35m'      # Purple
Cyan='\033[0;36m'         # Cyan
White='\033[0;37m'        # White

# Bold
BoldRed='\033[1;31m'         # Red
BoldGreen='\033[1;32m'       # Green
BoldYellow='\033[1;33m'      # Yellow
BoldBlue='\033[1;34m'        # Blue
BoldMagenta='\033[1;35m'     # Purple
BoldCyan='\033[1;36m'        # Cyan
BoldWhite='\033[1;37m'       # White

BUILDTYPE='Debug'
C_COMPILER='clang-18'     # gcc
CXX_COMPILER='clang++-18' # g++

clear
rm -r build
mkdir build
cd build
echo -e "--${BoldMagenta} BUILDING STURDR++${Reset}";

case "$OSTYPE" in
  linux*)
    echo -e "--${BoldMagenta} OS: linux${Reset}";
    cmake .. \
        -DINSTALL_NAVTOOLS_EXAMPLES=False \
        -DCMAKE_INSTALL_PREFIX=../build \
        -DCMAKE_BUILD_TYPE=$BUILDTYPE \
        -DCMAKE_C_COMPILER=$C_COMPILER \
        -DCMAKE_CXX_COMPILER=$CXX_COMPILER \
        ;;
  darwin*)
    echo -e "${BoldMagenta}-- OS: mac${Reset}"; 
    cmake .. \
        -DINSTALL_NAVTOOLS_EXAMPLES=False \
        -DCMAKE_INSTALL_PREFIX=../build \
        -DCMAKE_BUILD_TYPE=$BUILDTYPE
        -DCMAKE_C_COMPILER=$C_COMPILER \
        -DCMAKE_CXX_COMPILER=$CXX_COMPILER \
        ;;
  msys*)
    echo -e "${BoldMagenta}-- OS: windows${Reset}";
    cmake .. \
        -G "MinGW Makefiles" \
        -DCMAKE_CXX_COMPILER=C:/MinGW/bin/g++.exe \
        -DCMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe \
        -DINSTALL_NAVTOOLS_EXAMPLES=False \
        -DCMAKE_INSTALL_PREFIX=../build \
        -DCMAKE_BUILD_TYPE=$BUILDTYPE
        ;;
  solaris*)
    echo -e "${BoldMagenta}-- OS: solaris${Reset}";;
  bsd*)
    echo -e "${BoldMagenta}-- OS: bsd${Reset}";;
  *)
    echo -e "${BoldMagenta}-- OS: unknown${Reset}";;
esac

cmake --build . -- -j4
# make
# make install
cd ..