#!/bin/env bash

if [ $# -lt 1 ]; then
    PYTHON_MAJOR_REQ=3
else
    PYTHON_MAJOR_REQ=$1
fi

WINPYTHON_DOWNLOAD=winPython_3.6/3.6.1.0/WinPython-64bit-3.6.1.0
WINPYTHON_VERSION=python-3.6.1.amd64
#WINPYTHON_DOWNLOAD=winPython_3.4/3.4.4.1/WinPython-64bit-3.4.4.1Qt5
#WINPYTHON_VERSION=python-3.4.4.amd64

searchPython(){
    FindRunDone=$1
    if command -v python 1>/dev/null 2>/dev/null; then
        PYTHONVERSION=`python -c 'import sys; print(sys.version)'`
        PYTHONMAJOR=`python -c 'import sys; print(sys.version_info.major)'`
        PYTHONMINOR=`python -c 'import sys; print(sys.version_info.minor)'`
        
        echo "PYTHON FOUND:" $PYTHONVERSION $PYTHONMAJOR
        echo "VERSION:" $PYTHONMAJOR.$PYTHONMINOR
        
        if [ -z "$FindRunDone" ]; then
            # obvious python is allready path
            rm -rf $GIMLI_ROOT/.bash_hint_python
        fi
    else
        if [ -z "$FindRunDone" ]; then
            echo "---------------------------------------------------------------------------------------------------"
            echo "Please install http://sourceforge.net/projects/winpython/files/$WINPYTHON_DOWNLOAD'.exe'/download"
            echo "We try to find it if you already have but forget to set a proper path" 
            echo "---------------------------------------------------------------------------------------------------"
            echo "No suiteable python version found .. trying to find one .......... find /C -name $WINPYTHON_VERSION"
            
            ar=($(find /C -name "$WINPYTHON_VERSION" 2>/dev/null))
            if [ -n "$ar" ]; then
                echo "Found: " $ar
                echo "Setting temporarily the PATH variable."
                echo "If you want to store these PATH settings permanently (I bet you want)."
                echo "You need to paste the following into your $HOME/.bashrc "
                echo "-----------------------------------------------"
                echo "export PATH=\$PATH:$ar"
                echo "export PATH=\$PATH:$ar" > $GIMLI_ROOT/.bash_hint_python
                echo "-----------------------------------------------"
                export PATH=$PATH:$ar
            fi
            searchPython 1
        else
  
            echo "-----------------------------------------------------------------------------------"
            echo "No suiteable python version found .. you need to install winpython"
            echo "http://sourceforge.net/projects/winpython/files/$WINPYTHON_DOWNLOAD'.exe'/download"
            echo "Please install winpython"
            echo "You can the simple rerun this script and find you Python installation temporarily, "
            echo "or set your PATH variable to your winpython installation directory."
        fi
    fi
}

patchPython(){
    pacman -S --needed --noconfirm diffutils patch
        
    PYPATH=`which python`
    PYCONFIGFILE=${PYPATH%/python}/include/pyconfig.h
    echo "backup $PYCONFIGFILE to $PYCONFIGFILE.backup"
    [ ! -f $PYCONFIGFILE.backup ] && cp $PYCONFIGFILE $PYCONFIGFILE.backup
    cp $PYCONFIGFILE.backup $PYCONFIGFILE
    
    LINE=`grep -n '#define COMPILER "\[gcc\]"' $PYCONFIGFILE | cut -d':' -f1`
    LINE=$[$LINE + 1]
    echo "patch hypot gcc bug at line: $LINE"
    sed -i ''"$LINE"'s/\#define hypot _hypot/\/\/\#define hypot _hypot\n\#include <cmath>/' $PYCONFIGFILE
    echo "Difference is:"
    diff $PYCONFIGFILE $PYCONFIGFILE.backup
}

searchPython
patchPython