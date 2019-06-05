#include <iostream>
#include <vector>
#include <string>

#define BOOST_TEST_MODULE QueryTest
#include <boost/test/included/unit_test.hpp>

#include "M6Lib.h"
#include "M6Tokenizer.h"
#include "M6Iterator.h"
#include "M6Databank.h"

using namespace std;


int VERBOSE = 0;


BOOST_AUTO_TEST_CASE(TestParse1)
{
    M6Tokenizer tokenizer(std::string("resolution < 1.2"));

    BOOST_CHECK_EQUAL(tokenizer.GetNextQueryToken(), eM6TokenWord);
    BOOST_CHECK_EQUAL(tokenizer.GetTokenString(), "resolution");

    BOOST_CHECK_EQUAL(tokenizer.GetNextQueryToken(), eM6TokenLessThan);

    BOOST_CHECK_EQUAL(tokenizer.GetNextQueryToken(), eM6TokenFloat);
    BOOST_CHECK_EQUAL(tokenizer.GetTokenString(), "1.2");
}

BOOST_AUTO_TEST_CASE(TestParse2)
{
    M6Tokenizer tokenizer("resolution<1.2");

    BOOST_CHECK_EQUAL(tokenizer.GetNextQueryToken(), eM6TokenWord);
    BOOST_CHECK_EQUAL(tokenizer.GetTokenString(), "resolution");

    BOOST_CHECK_EQUAL(tokenizer.GetNextQueryToken(), eM6TokenLessThan);

    BOOST_CHECK_EQUAL(tokenizer.GetNextQueryToken(), eM6TokenFloat);
    BOOST_CHECK_EQUAL(tokenizer.GetTokenString(), "1.2");
}

