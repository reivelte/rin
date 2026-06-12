// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QApplication>
#include "window.hpp"

int main(int argc, char** argv) 
{
    QApplication app(argc, argv);
    rin::main_window window;
    window.show();
    return app.exec();
}