# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#! [cmake_use]
find_package(Qt6 REQUIRED COMPONENTS Xml)
target_link_libraries(mytarget PRIVATE Qt6::Xml)
#! [cmake_use]
