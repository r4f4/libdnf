#include "ModuleTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(ModuleTest);

#include "libdnf/dnf-module.hpp"
#include "libdnf/dnf-context.hpp"
#include "libdnf/dnf-repo-loader.h"
#include "libdnf/conf/ConfigParser.hpp"
#include "libdnf/conf/OptionBool.hpp"

using namespace libdnf;

void ModuleTest::setUp()
{
    GError *error = nullptr;
    context = dnf_context_new();

    dnf_context_set_release_ver(context, "f26");
    dnf_context_set_arch(context, "x86_64");
    constexpr auto install_root = TESTDATADIR "/modules/";
    dnf_context_set_install_root(context, install_root);
    constexpr auto repos_dir = TESTDATADIR "/modules/yum.repos.d/";
    dnf_context_set_repo_dir(context, repos_dir);
    dnf_context_set_solv_dir(context, "/tmp");
    dnf_context_setup(context, nullptr, &error);
    g_assert_no_error(error);

    dnf_context_setup_sack(context, dnf_context_get_state(context), &error);
    g_assert_no_error(error);

    auto loader = dnf_context_get_repo_loader(context);
    auto repo = dnf_repo_loader_get_repo_by_id(loader, "test", &error);
    g_assert_no_error(error);
    g_assert(repo != nullptr);
}

void ModuleTest::tearDown()
{
    g_object_unref(context);
}

#if 0
void ModuleTest::testDummy()
{
    std::cout << "called ModuleTest::testDummy()" << std::endl;

    /* call with empty module list should do nothing */
    {
        std::vector<std::string> module_list;
        CPPUNIT_ASSERT(dnf_module_dummy(module_list));
    }

    {
        std::vector<std::string> module_list;
        /* add some modules to the list and try again */
        module_list.push_back(std::string("moduleA"));
        module_list.push_back(std::string("moduleB:streamB"));
        module_list.push_back(std::string("moduleC:streamC/profileC"));

        CPPUNIT_ASSERT(dnf_module_dummy(module_list));
    }
}
#endif

void ModuleTest::testEnable()
{
    GPtrArray *repos = dnf_context_get_repos(context);
    auto sack = dnf_context_get_sack(context);
    auto install_root = dnf_context_get_install_root(context);

    std::cout << "called ModuleTest::testEnable()" << std::endl;

    /* call with empty module list should throw exception */
    {
        std::cout << "empty module list" << std::endl;
        std::vector<std::string> module_list{};
        CPPUNIT_ASSERT_THROW(dnf_module_enable(module_list, sack, repos, install_root), ModuleCommandException);
    }

    /* call with inexistent module spec should throw exception */
    {
        std::cout << "inexistent module" << std::endl;
        std::vector<std::string> module_list{"moduleA"};
        CPPUNIT_ASSERT_THROW(dnf_module_enable(module_list, sack, repos, install_root), ModuleException);
    }

    /* call with invalid specs should throw exceptions */
    {
        std::cout << "invalid specs" << std::endl;
        std::vector<std::string> module_list{
            "moduleA#wrong", "moduleB:streamB#wrong",
            "moduleC:streamC:versionC#wrong"
        };

        try {
            dnf_module_enable(module_list, sack, repos, install_root);
        } catch (ModuleException & e) {
            CPPUNIT_ASSERT(e.list().size() == 3);
            /*
            for (const auto & ex : e.list()) {
                std::cout << ex.what() << std::endl;
            }
            */
        }
    }

    /* call with valid module specs should succeed */
    {
        std::cout << "valid module specs" << std::endl;
        ConfigParser parser;
        OptionBool enabled{false};

        parser.read(TESTDATADIR "/modules/etc/dnf/modules.d/httpd.module");
        CPPUNIT_ASSERT(enabled.fromString(parser.getValue("httpd", "enabled")) == false);

        parser.read(TESTDATADIR "/modules/etc/dnf/modules.d/base-runtime.module");
        CPPUNIT_ASSERT(enabled.fromString(parser.getValue("base-runtime", "enabled")));

        std::vector<std::string> module_list{"httpd", "base-runtime"};
        CPPUNIT_ASSERT(dnf_module_enable(module_list, sack, repos, install_root));

        parser.read(TESTDATADIR "/modules/etc/dnf/modules.d/httpd.module");
        CPPUNIT_ASSERT(enabled.fromString(parser.getValue("httpd", "enabled")));

        parser.read(TESTDATADIR "/modules/etc/dnf/modules.d/base-runtime.module");
        CPPUNIT_ASSERT(enabled.fromString(parser.getValue("base-runtime", "enabled")));
    }

    /* call with module:stream spec should succeed */
    {
        std::cout << "module:stream" << std::endl;
        std::vector<std::string> module_list{"httpd:2.4"};

        CPPUNIT_ASSERT(dnf_module_enable(module_list, sack, repos, install_root));

        /* FIXME: check right stream is enabled */
    }

    /* call with module:stream/profile should succeed */
    {
        std::cout << "module:stream/profile" << std::endl;
        std::vector<std::string> module_list{"httpd:2.2/doc"};

        CPPUNIT_ASSERT(dnf_module_enable(module_list, sack, repos, install_root));

        /* FIXME: check right stream/profile are enabled */
    }

    g_object_unref(repos);
}
