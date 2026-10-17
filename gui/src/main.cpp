// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QApplication>
#include <suzuri/core/ctl.hpp>
#include "window.hpp"

int main(int argc, char** argv) 
{
    int exit_code = 0;
    sz::core_ctl::init(argc, argv);
    {
        QApplication app(argc, argv);
        rin::main_window window;
        window.show();
        exit_code = app.exec();
    }
    sz::core_ctl::exit();
    return exit_code;
}