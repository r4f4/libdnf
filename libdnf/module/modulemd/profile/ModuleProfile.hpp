/*
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef LIBDNF_MODULEPROFILE_HPP
#define LIBDNF_MODULEPROFILE_HPP


#include <memory>

#include <modulemd/modulemd.h>

#include "Profile.hpp"

class ModuleProfile : public Profile
{
public:
    explicit ModuleProfile(ModulemdProfile *profile);
    ~ModuleProfile() override = default;

    std::string getName() const override;
    std::string getDescription() const override;
    std::vector<std::string> getContent() const override;

private:
    ModulemdProfile *profile;
};


#endif //LIBDNF_MODULEPROFILE_HPP
