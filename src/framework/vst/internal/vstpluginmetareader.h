/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore BVBA and others
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

#ifndef MUSE_VST_VSTPLUGINMETAREADER_H
#define MUSE_VST_VSTPLUGINMETAREADER_H

#include "audioplugins/iaudiopluginmetareader.h"

namespace muse::vst {
class VstPluginMetaReader : public audioplugins::IAudioPluginMetaReader
{
public:
    audio::AudioResourceType metaType() const override;
    bool canReadMeta(const io::path_t& pluginPath) const override;
    RetVal<muse::audio::AudioResourceMetaList> readMeta(const io::path_t& pluginPath) const override;
};
}

#endif // MUSE_VST_VSTPLUGINMETAREADER_H
