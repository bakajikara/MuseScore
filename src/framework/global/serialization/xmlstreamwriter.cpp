/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "xmlstreamwriter.h"

#include <list>

#include <global/thirdparty/utfcpp-3.2.1/utf8.h>

#include "global/containers.h"
#include "textstream.h"

#include "global/log.h"

using namespace std::literals;
using namespace muse;

struct XmlStreamWriter::Impl {
    std::list<std::string> stack;
    TextStream stream;

    void putLevel()
    {
        size_t level = stack.size();
        for (size_t i = 0; i < level * 2; ++i) {
            stream << ' ';
        }
    }
};

XmlStreamWriter::XmlStreamWriter()
{
    m_impl = new Impl();
}

XmlStreamWriter::XmlStreamWriter(io::IODevice* dev)
{
    m_impl = new Impl();
    m_impl->stream.setDevice(dev);
}

XmlStreamWriter::~XmlStreamWriter()
{
    flush();
    delete m_impl;
}

void XmlStreamWriter::setDevice(io::IODevice* dev)
{
    m_impl->stream.setDevice(dev);
}

void XmlStreamWriter::flush()
{
    m_impl->stream.flush();
}

void XmlStreamWriter::startDocument()
{
    m_impl->stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}

void XmlStreamWriter::writeDoctype(const String& type)
{
    m_impl->stream << "<!DOCTYPE " << type << '>' << '\n';
}

std::string XmlStreamWriter::escapeCodePoint(const char32_t c)
{
    switch (c) {
    case U'<':
        return "&lt;"s;
    case U'>':
        return "&gt;"s;
    case U'&':
        return "&amp;"s;
    case U'\"':
        return "&quot;"s;
    default:
        // ignore invalid characters in xml 1.0
        if ((c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)) {
            return std::string();
        }
        return utf8::utf32to8(std::u32string_view(&c, 1));
    }
}

std::string XmlStreamWriter::escapeString(const std::string_view str)
{
    std::string escaped{};
    auto it = str.begin();
    while (it != str.end()) {
        const char32_t c = utf8::next(it, str.end());
        escaped += escapeCodePoint(c);
    }

    return escaped;
}

void XmlStreamWriter::writeValue(const Value& v)
{
    // std::monostate, int, unsigned int, signed long int, unsigned long int, signed long long, unsigned long long,
    // double, const char*, AsciiStringView, String
    switch (v.index()) {
    case 0:
        break;
    case 1: m_impl->stream << int32_t{ std::get<int>(v) };
        break;
    case 2: m_impl->stream << uint32_t{ std::get<unsigned int>(v) };
        break;
    case 3: m_impl->stream << int64_t{ std::get<signed long int>(v) };
        break;
    case 4: m_impl->stream << uint64_t{ std::get<unsigned long int>(v) };
        break;
    case 5: m_impl->stream << int64_t{ std::get<signed long long>(v) };
        break;
    case 6: m_impl->stream << uint64_t{ std::get<unsigned long long>(v) };
        break;
    case 7: m_impl->stream << std::get<double>(v);
        break;
    case 8: m_impl->stream << escapeString(AsciiStringView(std::get<const char*>(v)));
        break;
    case 9: m_impl->stream << escapeString(std::get<AsciiStringView>(v));
        break;
    case 10: m_impl->stream << escapeString(std::get<String>(v).toStdString());
        break;
    default:
        LOGI() << "index: " << v.index();
        UNREACHABLE;
        break;
    }
}

void XmlStreamWriter::startElement(const AsciiStringView& name, const Attributes& attrs)
{
    IF_ASSERT_FAILED(!name.contains(' ')) {
    }

    m_impl->putLevel();
    m_impl->stream << '<' << name;
    for (const Attribute& a : attrs) {
        m_impl->stream << ' ' << a.first << '=' << '\"';
        writeValue(a.second);
        m_impl->stream << '\"';
    }
    m_impl->stream << '>' << '\n';
    m_impl->stack.push_back(name.ascii());
}

void XmlStreamWriter::startElement(const String& name, const Attributes& attrs)
{
    ByteArray ba = name.toAscii();
    startElement(AsciiStringView(ba.constChar(), ba.size()), attrs);
}

void XmlStreamWriter::startElementRaw(const String& name)
{
    m_impl->putLevel();
    m_impl->stream << '<' << name << '>' << '\n';
    m_impl->stack.push_back(name.split(' ')[0].toStdString());
}

void XmlStreamWriter::endElement()
{
    m_impl->putLevel();
    m_impl->stream << "</" << muse::takeLast(m_impl->stack) << '>' << '\n';
    flush();
}

// <element attr="value" />
void XmlStreamWriter::element(const AsciiStringView& name, const Attributes& attrs)
{
    IF_ASSERT_FAILED(!name.contains(' ')) {
    }

    m_impl->putLevel();
    m_impl->stream << '<' << name;
    for (const Attribute& a : attrs) {
        m_impl->stream << ' ' << a.first << '=' << '\"';
        writeValue(a.second);
        m_impl->stream << '\"';
    }
    m_impl->stream << "/>\n";
}

void XmlStreamWriter::element(const AsciiStringView& name, const Value& body)
{
    IF_ASSERT_FAILED(!name.contains(' ')) {
    }

    m_impl->putLevel();
    m_impl->stream << '<' << name << '>';
    writeValue(body);
    m_impl->stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::element(const AsciiStringView& name, const Attributes& attrs, const Value& body)
{
    IF_ASSERT_FAILED(!name.contains(' ')) {
    }

    m_impl->putLevel();
    m_impl->stream << '<' << name;
    for (const Attribute& a : attrs) {
        m_impl->stream << ' ' << a.first << '=' << '\"';
        writeValue(a.second);
        m_impl->stream << '\"';
    }
    m_impl->stream << ">";
    writeValue(body);
    m_impl->stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::elementRaw(const String& nameWithAttributes, const Value& body)
{
    m_impl->putLevel();
    if (body.index() == 0) {
        m_impl->stream << '<' << nameWithAttributes << "/>\n";
    } else {
        String ename(String(nameWithAttributes).split(' ')[0]);
        m_impl->stream << '<' << nameWithAttributes << '>';
        writeValue(body);
        m_impl->stream << "</" << ename << '>' << '\n';
    }
}

void XmlStreamWriter::elementStringRaw(const String& nameWithAttributes, const String& body)
{
    m_impl->putLevel();
    if (body.isEmpty()) {
        m_impl->stream << '<' << nameWithAttributes << "/>\n";
    } else {
        String ename(String(nameWithAttributes).split(' ')[0]);
        m_impl->stream << '<' << nameWithAttributes << '>';
        m_impl->stream << body;
        m_impl->stream << "</" << ename << '>' << '\n';
    }
}

void XmlStreamWriter::comment(const String& text)
{
    m_impl->putLevel();
    m_impl->stream << "<!-- " << text << " -->\n";
}
