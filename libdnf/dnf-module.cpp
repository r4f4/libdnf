/*
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * SECTION:dnf-module
 * @short_description: Module management
 * @include: libdnf.h
 * @stability: Unstable
 *
 * High level interface for managing modules
 */

#include <iostream>
#include <assert.h>

#include "nsvcap.hpp"
#include "hy-subject.h"
#include "hy-util.h"
#include "dnf-module.hpp"
#include "conf/ConfigParser.hpp"
#include "conf/OptionBool.hpp"
#include "module/ModulePackage.hpp"
#include "module/ModulePackageMaker.hpp"
#include "module/ModulePackageContainer.hpp"
#include "module/PlatformModulePackage.hpp"
#include "module/modulemd/ModuleDefaultsContainer.hpp"
#include "utils/File.hpp"
#include "utils/utils.hpp"

namespace {

std::string getFileContent(const std::string &path)
{
    auto yaml = libdnf::File::newFile(path);
    yaml->open("r");
    const auto &yamlContent = yaml->getContent();
    yaml->close();

    return yamlContent;
}

void createConflictsBetweenStreams(const std::map<Id, std::shared_ptr<ModulePackage>> &modules)
{
    for (const auto &iter : modules) {
        const auto &modulePackage = iter.second;

        for (const auto &innerIter : modules) {
            if (modulePackage->getName() == innerIter.second->getName() && modulePackage->getStream() != innerIter.second->getStream()) {
                modulePackage->addStreamConflict(innerIter.second);
            }
        }
    }
}

void readModuleMetadataFromRepos(const GPtrArray *repos,
                                 ModulePackageContainer &modulePackages, ModuleDefaultsContainer &moduleDefaults)
{
    auto pool = modulePackages.getPool();
    DnfRepo *systemRepo = nullptr;

    for (unsigned int i = 0; i < repos->len; i++) {
        auto repo = static_cast<DnfRepo *>(g_ptr_array_index(repos, i));
        if (strcmp(dnf_repo_get_id(repo), HY_SYSTEM_REPO_NAME) == 0)
            systemRepo = repo;

        auto modules_fn = dnf_repo_get_filename_md(repo, "modules");
        if (modules_fn == nullptr)
            continue;

        const auto &yamlContent = getFileContent(modules_fn);

        auto modules = ModulePackageMaker::fromString(pool.get(), dnf_repo_get_repo(repo), yamlContent);
        createConflictsBetweenStreams(modules);

        modulePackages.add(modules);

        // update defaults from repo
        try {
            moduleDefaults.fromString(yamlContent, 0);
        } catch (ModuleDefaultsContainer::ConflictException &exception) {
            std::cerr << exception.what() << std::endl;
        }
    }

    if (systemRepo != nullptr) {
        char *arch;
        hy_detect_arch(&arch);

        // TODO remove hard-coded path
        PlatformModulePackage::createSolvable(pool.get(), dnf_repo_get_repo(systemRepo), "/etc/os-release", arch);
        free(arch);
    }
}

void readModuleDefaultsFromDisk(const std::string &path, ModuleDefaultsContainer &moduleDefaults)
{
    for (const auto &file : filesystem::getDirContent(path)) {
        const auto &yamlContent = getFileContent(file);

        try {
            moduleDefaults.fromString(yamlContent, 1000);
        } catch (ModuleDefaultsContainer::ConflictException &exception) {
            std::cerr << exception.what() << std::endl;
        }
    }
}

void enableModuleStreams(ModulePackageContainer &modulePackages, const char *install_root)
{
    // TODO: remove hard-coded path
    std::string dirPath = g_build_filename(install_root, "/etc/dnf/modules.d/", NULL);

    libdnf::ConfigParser parser{};
    for (const auto &file : filesystem::getDirContent(dirPath)) {
        //std::cout << "Reading file " << file << std::endl;
        parser.read(file);
    }

    for (const auto &iter : parser.getData()) {
        const auto &name = iter.first;
        libdnf::OptionBool enabled{false};

        if (!enabled.fromString(parser.getValue(name, "enabled"))) {
            continue;
        }
        const auto &stream = parser.getValue(name, "stream");
        modulePackages.enable(name, stream);
    }
}

void setupModules(const GPtrArray *repos, const char *install_root,
                  ModulePackageContainer &modules,
                  ModuleDefaultsContainer &moduleDefaults)
{
    /* FIXME: removed hardcoded path - get this from conf file */
    const std::string defaultsDirPath = g_build_filename(install_root, "/etc/dnf/modules.defaults.d/", NULL);

    readModuleMetadataFromRepos(repos, modules, moduleDefaults);
    readModuleDefaultsFromDisk(defaultsDirPath, moduleDefaults);

    moduleDefaults.resolve();

    // get default module streams from repos and disk
    auto defaultStreams = moduleDefaults.getDefaultStreams();
    enableModuleStreams(modules, install_root);

    //std::vector<std::shared_ptr<ModulePackage>> activeModulePackages{modules.getActiveModulePackages(defaultStreams)};
}

static bool
dnf_module_parse_spec(const std::string &module_spec, libdnf::Nsvcap &spec)
{
    spec.clear();

    for (std::size_t i = 0;
         HY_MODULE_FORMS_MOST_SPEC[i] != _HY_MODULE_FORM_STOP_;
         ++i) {
        auto form = HY_MODULE_FORMS_MOST_SPEC[i];

        if (spec.parse(module_spec.c_str(), form)) {
            /*
            std::cout << "Name: " << spec.getName() << std::endl;
            std::cout << "Stream: " << spec.getStream() << std::endl;
            std::cout << "Profile: " << spec.getProfile() << std::endl;
            std::cout << "Version: " << spec.getVersion() << std::endl;
            */

            return true;
        }
    }

    return false;
}

bool
dnf_module_dummy(const std::vector<std::string> & module_list)
{
    for (auto module_spec : module_list) {
        std::cerr << "module " << module_spec << std::endl;
    }

    return true;
}

std::string findAppropriateStream(std::shared_ptr<ModulePackage> &module,
                                  const std::string &userStream, ModuleDefaultsContainer &moduleDefaults, const char *install_root)
{
    const auto &name = module->getName();

    /* Try to use user supplied stream name */
    if (userStream != "") {
        std::cerr << "User stream: " << userStream << std::endl;
        return userStream;
    }

    std::string stream{};

    /* Try to use stream from config file */
    libdnf::ConfigParser parser{};
    std::ostringstream oss;
    oss << name << ".module";
    const std::string confPath = g_build_filename(install_root, "/etc/dnf/modules.d/", oss.str().c_str(), NULL);

    if (filesystem::exists(confPath)) {
        parser.read(confPath);

        libdnf::OptionBool enabled{false};
        //if (enabled.fromString(parser.getValue(name, "enabled"))) {
            stream = parser.getValue(name, "stream");
            if (stream != "") {
                std::cerr << "Conf stream: " << stream << std::endl;
                return stream;
            }
        //}
    } else {
        std::cerr << confPath << " does not exist" << std::endl;
    }

    /* Try to use default stream */
    stream = moduleDefaults.getDefaultStreamFor(name);
    if (stream != "") {
        std::cerr << "Def stream: " << stream << std::endl;
        return stream;
    }

    throw ModulePackageContainer::EnabledStreamException(name);
}

} /* namespace */

namespace libdnf {

bool
dnf_module_enable(const std::vector<std::string> &spec_list,
                  DnfSack *sack, GPtrArray *repos, const char *install_root)
{
    ModuleExceptionList exceptions;

    if (spec_list.empty()) {
        throw ModuleCommandException("module_list cannot be null");
    }

    char *arch;
    hy_detect_arch(&arch);

    for (const auto& module_spec : spec_list) {
        libdnf::Nsvcap spec;

        std::cerr << "enabling '" << module_spec <<"'" << std::endl;

        if (!dnf_module_parse_spec(module_spec, spec)) {
            std::ostringstream oss;
            oss << module_spec << " is not a valid spec";
            exceptions.push_back(ModuleCommandException(oss.str()));
            continue;
        }

        ModulePackageContainer modules{std::shared_ptr<Pool>(pool_create(), &g_object_unref), arch};
        ModuleDefaultsContainer moduleDefaults;
        setupModules(repos, install_root, modules, moduleDefaults);

        std::shared_ptr<ModulePackage> module;
        try {
            module = modules.getModulePackage(spec.getName());
        } catch (ModulePackageContainer::NoModuleException &e) {
            exceptions.push_back(ModuleCommandException(e.what()));
            continue;
        }

        /* FIXME: should we check the stream? */
        if (module->isEnabled()) {
            std::cerr << module->getName() << " is already enabled" << std::endl;
            continue;
        } else {
            std::cerr << module->getName() << " is not enabled" << std::endl;
        }

        /* FIXME: resolve module deps (TBD)
         * FIXME: enable module dependencies
        for (auto dep : get_module_dependency_latest(name, stream))
            dnf_module_enable(name, stream)
        */

        std::string stream{};
        try {
            stream = findAppropriateStream(module, spec.getStream(), moduleDefaults, install_root);
        } catch (ModulePackageContainer::EnabledStreamException &e) {
            exceptions.push_back(ModuleCommandException(e.what()));
        }

        std::cerr << stream << " vs " << module->getStream() << std::endl;

        std::cerr << "Enable " << module->getName() << ":" << stream << std::endl;

        /* FIXME: this does nothing because metadata->stream != stream */
        modules.enable(module->getName(), stream);
        module->enable();
        assert(module->isEnabled());

        std::ofstream ofs;
        std::ostringstream oss;
        oss << module->getName() << ".module";
        const std::string cfgfile = g_build_filename(install_root, "/etc/dnf/modules.d/", oss.str().c_str(), NULL);

        std::cerr << "Writing to " << cfgfile << std::endl;

        ofs.open(cfgfile, std::ofstream::trunc);
        ofs << "[" << module->getName() << "]\n";
        ofs << "name = " << module->getName() << "\n";
        ofs << "stream = " << stream << "\n";
        ofs << "version = " << module->getVersion() << "\n";
        /* FIXME: just write installed profile */
        const auto profiles = module->getProfiles();
        ofs << "profiles = ";
        for (const auto &profile : profiles)
            ofs << profile->getName() << ",";
        ofs << "\n";
        ofs << "enabled = ";
        if (module->isEnabled())
            ofs << "true";
        else
            ofs << "false";
        ofs << "\n";
        /* FIXME: get this info */
        ofs << "locked = false\n";

        ofs.close();

        /* FIXME: where are we getting sack, repos and install_root from? */
        dnf_sack_filter_modules(sack, repos, install_root);

    }

    free(arch);

    if (!exceptions.empty())
        throw ModuleException(exceptions);

    return true;
}

} /* namespace libdnf */
