/*
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __DNF_MODULE_H
#define __DNF_MODULE_H

#include <vector>
#include <string>
#include <sstream>
#include <memory>

#include "dnf-types.h"

namespace libdnf {

struct ModuleCommandException : public std::runtime_error {
    explicit ModuleCommandException(const std::string &what) : std::runtime_error(what) {}
};

typedef std::vector<ModuleCommandException> ModuleExceptionList;

class ModuleException : public std::exception {
public:
    explicit ModuleException(const ModuleExceptionList &what) : e_list(std::move(what)) {}
    const ModuleExceptionList & list() const noexcept { return e_list; }
    const char* what() const noexcept { return e_list.front().what(); }

private:
    ModuleExceptionList e_list;
};

bool dnf_module_dummy(const std::vector<std::string> & module_list);
bool dnf_module_enable(const std::vector<std::string> &spec_list, DnfSack *sack, GPtrArray *repos, const char *install_root);

}

#endif /* __DNF_MODULE_H */
