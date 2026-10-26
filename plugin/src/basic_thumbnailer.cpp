// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <string>
#include <vector>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtCore/QByteArray>
#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QImageReader>
#include <suzuri/plugin/plugin.hpp>

// basic_thumbnailer.cpp
// can be compiled in rin's nix development shell using the following command on linux:
// g++ -rdynamic -shared -fPIC -ldl -lQt6Core -lQt6Gui -L {path/to/libsuzuri.so} -I {path to local sz headers} -std=c++23 -o {out name} {src.cpp}
// the compiled "plugin module" should be placed in $XDG_DATA_HOME/suzuri/plugins. on linux, this defaults to ~/.local/share/suzuri/plugins
// TODO: add functionality to build.py to enable building plugins under /plugin easily
// TODO: turn /plugin into a nix package to be able to ship a collection of default plugins with suzuri as needed

extern "C" std::vector<sz::plugin_descriptor> list();
extern "C" sz::native_plugin* create(int plugin_index);

static const sz::plugin_descriptor d0{
    .name = "basic_thumbnailer",
    .type = "thumbnailer",
    .version = "2026.10.22",
    .features{ "formats:jpg,jpeg,png,bmp" }
};

class basic_thumbnailer : public sz::thumbnailer
{
    public:
    basic_thumbnailer() { }
    ~basic_thumbnailer() { }

    sz::plugin_descriptor descriptor() const override { return d0; }

    std::string create_thumbnail(std::filesystem::path path, int width, int height) override
    {
        QImageReader reader(QString::fromStdString(path.string()));
        reader.setScaledSize({width, height});
        auto pm = QPixmap::fromImageReader(&reader);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        pm.save(&buffer, "PNG");
        return bytes.toStdString();
    }
};

std::vector<sz::plugin_descriptor> list()
{
    return { d0 };
}

sz::native_plugin* create(int plugin_index)
{
    switch (plugin_index)
    {
    default:
    case 0: return new basic_thumbnailer;
    }
    return nullptr;
}
