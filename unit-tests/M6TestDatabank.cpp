#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

#include <boost/filesystem.hpp>
#define BOOST_TEST_MODULE DatabankTest
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>

#include "M6Config.h"
#include "M6Query.h"
#include "M6Builder.h"
#include "M6Server.h"
#include "M6Databank.h"
#include "M6Lexicon.h"
#include "M6Document.h"
#include "M6Iterator.h"
#include "M6Databank.h"
#include "M6Builder.h"
#include "M6Server.h"

int VERBOSE = 0;

using namespace std;
namespace fs = boost::filesystem;


BOOST_AUTO_TEST_CASE(TestDatabank1)
{
    std::string dbName = "test-pdbfinder";

    M6Config::SetConfigFilePath("unit-tests/data/mrs-config.xml");

    if (not boost::filesystem::is_directory("test/mrs"))
        boost::filesystem::create_directory("test/mrs");

    M6Builder builder(dbName);
    builder.Build(1);

    const zx::element* config = M6Config::GetServer();
    M6Server server(config);

    std::vector<el::object> hits;
    uint32 hitCount;
    bool ranked;
    std::string parseError;

    server.Find(dbName, "1xxx", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);

    server.Find(dbName, "exp_method:x", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);

    server.Find(dbName, "Resolution < 1.2", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);

    server.Find(dbName, "Resolution > 1.2", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount <= 0);

    server.Find(dbName, "hssp_n_align > 100", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);
}

/BOOST_AUTO_TEST_CASE(test_databank_0)
//{
//    cout << "testing pdbfinder" << endl;
//
//    M6Databank databank("./test/pdbfinder.m6", eReadOnly);
//    databank.Validate();
//}

BOOST_AUTO_TEST_CASE(test_databank_1)
{
    cout << "testing pdbfinder" << endl;

//    boost::timer::auto_cpu_timer t;

    //M6Databank databank("./test/pdbfinder.m6", eReadWrite);
    //databank.RecalculateDocumentWeights();
}

// test of building a databank with a IDL index
BOOST_AUTO_TEST_CASE(test_databank_2)
{
    cout << "testing IDL 1" << endl;

    if (fs::exists("./test/test-idl.m6"))
        fs::remove_all("./test/test-idl.m6");

    M6Databank databank("./test/test-idl.m6", eReadWrite);

    M6Lexicon lexicon;
    databank.StartBatchImport(lexicon);

    string text("eerste aap noot mies, boom roos vis. vuur & water");

    M6InputDocument* doc = new M6InputDocument(databank, text);
    for (int i = 0; i < 100; ++i)
        doc->Index("text", eM6TextData, false, text.c_str(), text.length());
    doc->Tokenize(lexicon, 0);
    doc->Compress();
    databank.Store(doc);

    text = "tweede document aap noot mies, boom roos vis. vuur & water.";
    doc = new M6InputDocument(databank, text);
    for (int i = 0; i < 100; ++i)
        doc->Index("text", eM6TextData, false, text.c_str(), text.length());
    doc->Tokenize(lexicon, 0);
    doc->Compress();
    databank.Store(doc);

    databank.EndBatchImport();
    databank.FinishBatchImport();
}

// test of building a databank with a IDL index
BOOST_AUTO_TEST_CASE(test_databank_3)
{
    cout << "testing IDL 2" << endl;

    M6Databank databank("./test/test-idl.m6", eReadOnly);

    M6Iterator* iter = databank.FindString("text", "aap noot mies");
    BOOST_CHECK(iter != nullptr);

    uint32 docNr;
    float rank;

    BOOST_CHECK_EQUAL(iter->Next(docNr, rank), true);
    BOOST_CHECK_EQUAL(docNr, 1);
    BOOST_CHECK_EQUAL(iter->Next(docNr, rank), true);
    BOOST_CHECK_EQUAL(docNr, 2);
    BOOST_CHECK_EQUAL(iter->Next(docNr, rank), false);

    delete iter;

    iter = databank.FindString("text", "vuur & water");
    BOOST_CHECK(iter != nullptr);

    BOOST_CHECK_EQUAL(iter->Next(docNr, rank), true);
    BOOST_CHECK_EQUAL(docNr, 1);
    BOOST_CHECK_EQUAL(iter->Next(docNr, rank), true);
    BOOST_CHECK_EQUAL(docNr, 2);
    BOOST_CHECK_EQUAL(iter->Next(docNr, rank), false);

    delete iter;
}

BOOST_AUTO_TEST_CASE(test_databank_4)
{
    std::string dbName = "test-pdbfinder";

    M6Config::SetConfigFilePath("unit-tests/data/mrs-config.xml");

    if (not boost::filesystem::is_directory("test/mrs"))
        boost::filesystem::create_directory("test/mrs");

    M6Builder builder(dbName);
    builder.Build(1);

    const zx::element* config = M6Config::GetServer();
    M6Server server(config);

    std::vector<el::object> hits;
    uint32 hitCount;
    bool ranked;
    std::string parseError;

    server.Find(dbName, "1xxx", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);

    server.Find(dbName, "exp_method:x", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);

    server.Find(dbName, "Resolution < 1.2", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);

    server.Find(dbName, "Resolution > 1.2", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount <= 0);

    server.Find(dbName, "hssp_n_align > 100", true, 0, 10, true,
                hits, hitCount, ranked, parseError);
    BOOST_CHECK(hitCount > 0);
}
