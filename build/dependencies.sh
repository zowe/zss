#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

# This reads a project.proj.env file which states external compilation dependencies
# That can be downloaded via git. It either exits in failure or returns environment variables
# with suffix _DESTINATION for each dependency's path
#
# The format of the project file should be:
#
# PROJECT="myproject"
# VERSION="1.0.0"
# DEPS="DEP1"
# DEP1="nickname"
# DEP1_SOURCE="a-git-repo-address"
# DEP1_BRANCH="a-commit-branch-or-tag"
#
# For example,
#
# PROJECT="configmgr"
# VERSION="1.0.0"
# DEPS="QUICKJS LIBYAML"
#
# QUICKJS="quickjs"
# QUICKJS_SOURCE="git@github.com:joenemo/quickjs-portable.git"
# QUICKJS_BRANCH="main"
#
# LIBYAML="libyaml"
# LIBYAML_SOURCE="git@github.com:yaml/libyaml.git"
# LIBYAML_BRANCH="0.2.5"

check_dependencies() {
  echo "********************************************************************************"
  echo "Checking dependencies..."

  if [ $# -eq 0 ]; then
    echo "No dependencies given"
  elif [ $# -eq 2 ]; then
    echo "Checking dependencies in destination $1 using project file $2"
    if [ -e "$2" ]; then
      . "$2"
    else
      echo "File $2 not found, cannot check dependencies"
      exit 2
    fi
  elif [ $# -eq 1 ]; then
    echo "Checking dependencies in destination $1 using environment variables" 
  else
    echo "Usage $0 destination [project.env]"
    exit 1
  fi



  if [ -z "$DEPS" -o -z "$PROJECT" ]; then
    echo "DEPS or PROJECT variable not found, nothing to do."
    if [ -n "$2" ]; then
      echo "Dependencies file invalid format."
    fi
    exit 3
  else
    echo "Dependencies=$DEPS"
  fi

  destination=$1/deps/$PROJECT

  OLDIFS=$IFS
  IFS=" "
  for dep in ${DEPS}; do
    eval directory="\$${dep}"
    echo "Check if dir exist=$destination/$directory"
    if [ ! -d "${destination}/${directory}" ]; then
      eval echo "Clone: \$${dep}_SOURCE @ \$${dep}_BRANCH to \$${dep}"
      eval git clone --branch "\$${dep}_BRANCH" "\$${dep}_SOURCE" "${destination}/${directory}"
    else
      echo "The dependency already exists"
    fi
  done
  echo "Dependency setup complete"
  IFS=$OLDIFS
}

get_destination() {
  root=$1
  project=$2
  echo "$root/deps/$project"
}
get_dependency_destination() {
  root=$1
  project=$2
  dep=$3
  echo "$1/deps/$2/$3"
}
