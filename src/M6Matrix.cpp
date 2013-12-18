//   Copyright Maarten L. Hekkelman, Radboud University 2012.
//  Distributed under the Boost Software License, Version 1.0.
//     (See accompanying file LICENSE_1_0.txt or copy at
//           http://www.boost.org/LICENSE_1_0.txt)
//  Also distributed under the Lesser General Public License, Version 2.1.
//     (See accompanying file lgpl-2.1.txt or copy at
//           https://www.gnu.org/licenses/lgpl-2.1.txt)

#include "M6Lib.h"

#include <sstream>
#include <iostream>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
#include <boost/bind.hpp>

#include "M6Matrix.h"

using namespace std;
namespace io = boost::iostreams;

// --------------------------------------------------------------------

const int8 kMBlosum45[] = {
	  5,                                                                                                               // A
	 -1,   4,                                                                                                          // B
	 -1,  -2,  12,                                                                                                     // C
	 -2,   5,  -3,   7,                                                                                                // D
	 -1,   1,  -3,   2,   6,                                                                                           // E
	 -2,  -3,  -2,  -4,  -3,   8,                                                                                      // F
	  0,  -1,  -3,  -1,  -2,  -3,   7,                                                                                 // G
	 -2,   0,  -3,   0,   0,  -2,  -2,  10,                                                                            // H
	 -1,  -3,  -3,  -4,  -3,   0,  -4,  -3,   5,                                                                       // I
	 -1,   0,  -3,   0,   1,  -3,  -2,  -1,  -3,   5,                                                                  // K
	 -1,  -3,  -2,  -3,  -2,   1,  -3,  -2,   2,  -3,   5,                                                             // L
	 -1,  -2,  -2,  -3,  -2,   0,  -2,   0,   2,  -1,   2,   6,                                                        // M
	 -1,   4,  -2,   2,   0,  -2,   0,   1,  -2,   0,  -3,  -2,   6,                                                   // N
	 -1,  -2,  -4,  -1,   0,  -3,  -2,  -2,  -2,  -1,  -3,  -2,  -2,   9,                                              // P
	 -1,   0,  -3,   0,   2,  -4,  -2,   1,  -2,   1,  -2,   0,   0,  -1,   6,                                         // Q
	 -2,  -1,  -3,  -1,   0,  -2,  -2,   0,  -3,   3,  -2,  -1,   0,  -2,   1,   7,                                    // R
	  1,   0,  -1,   0,   0,  -2,   0,  -1,  -2,  -1,  -3,  -2,   1,  -1,   0,  -1,   4,                               // S
	  0,   0,  -1,  -1,  -1,  -1,  -2,  -2,  -1,  -1,  -1,  -1,   0,  -1,  -1,  -1,   2,   5,                          // T
	  0,  -3,  -1,  -3,  -3,   0,  -3,  -3,   3,  -2,   1,   1,  -3,  -3,  -3,  -2,  -1,   0,   5,                     // V
	 -2,  -4,  -5,  -4,  -3,   1,  -2,  -3,  -2,  -2,  -2,  -2,  -4,  -3,  -2,  -2,  -4,  -3,  -3,  15,                // W
	 -2,  -2,  -3,  -2,  -2,   3,  -3,   2,   0,  -1,   0,   0,  -2,  -3,  -1,  -1,  -2,  -1,  -1,   3,   8,           // Y
	 -1,   2,  -3,   1,   4,  -3,  -2,   0,  -3,   1,  -2,  -1,   0,  -1,   4,   0,   0,  -1,  -3,  -2,  -2,   4,      // Z
	  0,  -1,  -2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   0,   0,  -1,  -2,  -1,  -1,  -1, // x
};

const int8 kMBlosum50[] = {
	  5,                                                                                                               // A
	 -2,   5,                                                                                                          // B
	 -1,  -3,  13,                                                                                                     // C
	 -2,   5,  -4,   8,                                                                                                // D
	 -1,   1,  -3,   2,   6,                                                                                           // E
	 -3,  -4,  -2,  -5,  -3,   8,                                                                                      // F
	  0,  -1,  -3,  -1,  -3,  -4,   8,                                                                                 // G
	 -2,   0,  -3,  -1,   0,  -1,  -2,  10,                                                                            // H
	 -1,  -4,  -2,  -4,  -4,   0,  -4,  -4,   5,                                                                       // I
	 -1,   0,  -3,  -1,   1,  -4,  -2,   0,  -3,   6,                                                                  // K
	 -2,  -4,  -2,  -4,  -3,   1,  -4,  -3,   2,  -3,   5,                                                             // L
	 -1,  -3,  -2,  -4,  -2,   0,  -3,  -1,   2,  -2,   3,   7,                                                        // M
	 -1,   4,  -2,   2,   0,  -4,   0,   1,  -3,   0,  -4,  -2,   7,                                                   // N
	 -1,  -2,  -4,  -1,  -1,  -4,  -2,  -2,  -3,  -1,  -4,  -3,  -2,  10,                                              // P
	 -1,   0,  -3,   0,   2,  -4,  -2,   1,  -3,   2,  -2,   0,   0,  -1,   7,                                         // Q
	 -2,  -1,  -4,  -2,   0,  -3,  -3,   0,  -4,   3,  -3,  -2,  -1,  -3,   1,   7,                                    // R
	  1,   0,  -1,   0,  -1,  -3,   0,  -1,  -3,   0,  -3,  -2,   1,  -1,   0,  -1,   5,                               // S
	  0,   0,  -1,  -1,  -1,  -2,  -2,  -2,  -1,  -1,  -1,  -1,   0,  -1,  -1,  -1,   2,   5,                          // T
	  0,  -4,  -1,  -4,  -3,  -1,  -4,  -4,   4,  -3,   1,   1,  -3,  -3,  -3,  -3,  -2,   0,   5,                     // V
	 -3,  -5,  -5,  -5,  -3,   1,  -3,  -3,  -3,  -3,  -2,  -1,  -4,  -4,  -1,  -3,  -4,  -3,  -3,  15,                // W
	 -2,  -3,  -3,  -3,  -2,   4,  -3,   2,  -1,  -2,  -1,   0,  -2,  -3,  -1,  -1,  -2,  -2,  -1,   2,   8,           // Y
	 -1,   2,  -3,   1,   5,  -4,  -2,   0,  -3,   1,  -3,  -1,   0,  -1,   4,   0,   0,  -1,  -3,  -2,  -2,   5,      // Z
	 -1,  -1,  -2,  -1,  -1,  -2,  -2,  -1,  -1,  -1,  -1,  -1,  -1,  -2,  -1,  -1,  -1,   0,  -1,  -3,  -1,  -1,  -1, // x
};

const int8 kMBlosum62[] = {
	  4,                                                                                                               // A
	 -2,   4,                                                                                                          // B
	  0,  -3,   9,                                                                                                     // C
	 -2,   4,  -3,   6,                                                                                                // D
	 -1,   1,  -4,   2,   5,                                                                                           // E
	 -2,  -3,  -2,  -3,  -3,   6,                                                                                      // F
	  0,  -1,  -3,  -1,  -2,  -3,   6,                                                                                 // G
	 -2,   0,  -3,  -1,   0,  -1,  -2,   8,                                                                            // H
	 -1,  -3,  -1,  -3,  -3,   0,  -4,  -3,   4,                                                                       // I
	 -1,   0,  -3,  -1,   1,  -3,  -2,  -1,  -3,   5,                                                                  // K
	 -1,  -4,  -1,  -4,  -3,   0,  -4,  -3,   2,  -2,   4,                                                             // L
	 -1,  -3,  -1,  -3,  -2,   0,  -3,  -2,   1,  -1,   2,   5,                                                        // M
	 -2,   3,  -3,   1,   0,  -3,   0,   1,  -3,   0,  -3,  -2,   6,                                                   // N
	 -1,  -2,  -3,  -1,  -1,  -4,  -2,  -2,  -3,  -1,  -3,  -2,  -2,   7,                                              // P
	 -1,   0,  -3,   0,   2,  -3,  -2,   0,  -3,   1,  -2,   0,   0,  -1,   5,                                         // Q
	 -1,  -1,  -3,  -2,   0,  -3,  -2,   0,  -3,   2,  -2,  -1,   0,  -2,   1,   5,                                    // R
	  1,   0,  -1,   0,   0,  -2,   0,  -1,  -2,   0,  -2,  -1,   1,  -1,   0,  -1,   4,                               // S
	  0,  -1,  -1,  -1,  -1,  -2,  -2,  -2,  -1,  -1,  -1,  -1,   0,  -1,  -1,  -1,   1,   5,                          // T
	  0,  -3,  -1,  -3,  -2,  -1,  -3,  -3,   3,  -2,   1,   1,  -3,  -2,  -2,  -3,  -2,   0,   4,                     // V
	 -3,  -4,  -2,  -4,  -3,   1,  -2,  -2,  -3,  -3,  -2,  -1,  -4,  -4,  -2,  -3,  -3,  -2,  -3,  11,                // W
	 -2,  -3,  -2,  -3,  -2,   3,  -3,   2,  -1,  -2,  -1,  -1,  -2,  -3,  -1,  -2,  -2,  -2,  -1,   2,   7,           // Y
	 -1,   1,  -3,   1,   4,  -3,  -2,   0,  -3,   1,  -3,  -1,   0,  -1,   3,   0,   0,  -1,  -2,  -3,  -2,   4,      // Z
	  0,  -1,  -2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -2,  -1,  -1,   0,   0,  -1,  -2,  -1,  -1,  -1, // x
};

const int8 kMBlosum80[] = {
	  7,                                                                                                               // A
	 -3,   6,                                                                                                          // B
	 -1,  -6,  13,                                                                                                     // C
	 -3,   6,  -7,  10,                                                                                                // D
	 -2,   1,  -7,   2,   8,                                                                                           // E
	 -4,  -6,  -4,  -6,  -6,  10,                                                                                      // F
	  0,  -2,  -6,  -3,  -4,  -6,   9,                                                                                 // G
	 -3,  -1,  -7,  -2,   0,  -2,  -4,  12,                                                                            // H
	 -3,  -6,  -2,  -7,  -6,  -1,  -7,  -6,   7,                                                                       // I
	 -1,  -1,  -6,  -2,   1,  -5,  -3,  -1,  -5,   8,                                                                  // K
	 -3,  -7,  -3,  -7,  -6,   0,  -7,  -5,   2,  -4,   6,                                                             // L
	 -2,  -5,  -3,  -6,  -4,   0,  -5,  -4,   2,  -3,   3,   9,                                                        // M
	 -3,   5,  -5,   2,  -1,  -6,  -1,   1,  -6,   0,  -6,  -4,   9,                                                   // N
	 -1,  -4,  -6,  -3,  -2,  -6,  -5,  -4,  -5,  -2,  -5,  -4,  -4,  12,                                              // P
	 -2,  -1,  -5,  -1,   3,  -5,  -4,   1,  -5,   2,  -4,  -1,   0,  -3,   9,                                         // Q
	 -3,  -2,  -6,  -3,  -1,  -5,  -4,   0,  -5,   3,  -4,  -3,  -1,  -3,   1,   9,                                    // R
	  2,   0,  -2,  -1,  -1,  -4,  -1,  -2,  -4,  -1,  -4,  -3,   1,  -2,  -1,  -2,   7,                               // S
	  0,  -1,  -2,  -2,  -2,  -4,  -3,  -3,  -2,  -1,  -3,  -1,   0,  -3,  -1,  -2,   2,   8,                          // T
	 -1,  -6,  -2,  -6,  -4,  -2,  -6,  -5,   4,  -4,   1,   1,  -5,  -4,  -4,  -4,  -3,   0,   7,                     // V
	 -5,  -8,  -5,  -8,  -6,   0,  -6,  -4,  -5,  -6,  -4,  -3,  -7,  -7,  -4,  -5,  -6,  -5,  -5,  16,                // W
	 -4,  -5,  -5,  -6,  -5,   4,  -6,   3,  -3,  -4,  -2,  -3,  -4,  -6,  -3,  -4,  -3,  -3,  -3,   3,  11,           // Y
	 -2,   0,  -7,   1,   6,  -6,  -4,   0,  -6,   1,  -5,  -3,  -1,  -2,   5,   0,  -1,  -2,  -4,  -5,  -4,   6,      // Z
	 -1,  -3,  -4,  -3,  -2,  -3,  -3,  -2,  -2,  -2,  -2,  -2,  -2,  -3,  -2,  -2,  -1,  -1,  -2,  -5,  -3,  -1,  -2, // x
};

const int8 kMBlosum90[] = {
	  5,                                                                                                               // A
	 -2,   4,                                                                                                          // B
	 -1,  -4,   9,                                                                                                     // C
	 -3,   4,  -5,   7,                                                                                                // D
	 -1,   0,  -6,   1,   6,                                                                                           // E
	 -3,  -4,  -3,  -5,  -5,   7,                                                                                      // F
	  0,  -2,  -4,  -2,  -3,  -5,   6,                                                                                 // G
	 -2,  -1,  -5,  -2,  -1,  -2,  -3,   8,                                                                            // H
	 -2,  -5,  -2,  -5,  -4,  -1,  -5,  -4,   5,                                                                       // I
	 -1,  -1,  -4,  -1,   0,  -4,  -2,  -1,  -4,   6,                                                                  // K
	 -2,  -5,  -2,  -5,  -4,   0,  -5,  -4,   1,  -3,   5,                                                             // L
	 -2,  -4,  -2,  -4,  -3,  -1,  -4,  -3,   1,  -2,   2,   7,                                                        // M
	 -2,   4,  -4,   1,  -1,  -4,  -1,   0,  -4,   0,  -4,  -3,   7,                                                   // N
	 -1,  -3,  -4,  -3,  -2,  -4,  -3,  -3,  -4,  -2,  -4,  -3,  -3,   8,                                              // P
	 -1,  -1,  -4,  -1,   2,  -4,  -3,   1,  -4,   1,  -3,   0,   0,  -2,   7,                                         // Q
	 -2,  -2,  -5,  -3,  -1,  -4,  -3,   0,  -4,   2,  -3,  -2,  -1,  -3,   1,   6,                                    // R
	  1,   0,  -2,  -1,  -1,  -3,  -1,  -2,  -3,  -1,  -3,  -2,   0,  -2,  -1,  -1,   5,                               // S
	  0,  -1,  -2,  -2,  -1,  -3,  -3,  -2,  -1,  -1,  -2,  -1,   0,  -2,  -1,  -2,   1,   6,                          // T
	 -1,  -4,  -2,  -5,  -3,  -2,  -5,  -4,   3,  -3,   0,   0,  -4,  -3,  -3,  -3,  -2,  -1,   5,                     // V
	 -4,  -6,  -4,  -6,  -5,   0,  -4,  -3,  -4,  -5,  -3,  -2,  -5,  -5,  -3,  -4,  -4,  -4,  -3,  11,                // W
	 -3,  -4,  -4,  -4,  -4,   3,  -5,   1,  -2,  -3,  -2,  -2,  -3,  -4,  -3,  -3,  -3,  -2,  -3,   2,   8,           // Y
	 -1,   0,  -5,   0,   4,  -4,  -3,   0,  -4,   1,  -4,  -2,  -1,  -2,   4,   0,  -1,  -1,  -3,  -4,  -3,   4,      // Z
	 -1,  -2,  -3,  -2,  -2,  -2,  -2,  -2,  -2,  -1,  -2,  -1,  -2,  -2,  -1,  -2,  -1,  -1,  -2,  -3,  -2,  -1,  -2, // x
};

const int8 kMPam250[] = {
	  2,                                                                                                               // A
	  0,   3,                                                                                                          // B
	 -2,  -4,  12,                                                                                                     // C
	  0,   3,  -5,   4,                                                                                                // D
	  0,   3,  -5,   3,   4,                                                                                           // E
	 -3,  -4,  -4,  -6,  -5,   9,                                                                                      // F
	  1,   0,  -3,   1,   0,  -5,   5,                                                                                 // G
	 -1,   1,  -3,   1,   1,  -2,  -2,   6,                                                                            // H
	 -1,  -2,  -2,  -2,  -2,   1,  -3,  -2,   5,                                                                       // I
	 -1,   1,  -5,   0,   0,  -5,  -2,   0,  -2,   5,                                                                  // K
	 -2,  -3,  -6,  -4,  -3,   2,  -4,  -2,   2,  -3,   6,                                                             // L
	 -1,  -2,  -5,  -3,  -2,   0,  -3,  -2,   2,   0,   4,   6,                                                        // M
	  0,   2,  -4,   2,   1,  -3,   0,   2,  -2,   1,  -3,  -2,   2,                                                   // N
	  1,  -1,  -3,  -1,  -1,  -5,   0,   0,  -2,  -1,  -3,  -2,   0,   6,                                              // P
	  0,   1,  -5,   2,   2,  -5,  -1,   3,  -2,   1,  -2,  -1,   1,   0,   4,                                         // Q
	 -2,  -1,  -4,  -1,  -1,  -4,  -3,   2,  -2,   3,  -3,   0,   0,   0,   1,   6,                                    // R
	  1,   0,   0,   0,   0,  -3,   1,  -1,  -1,   0,  -3,  -2,   1,   1,  -1,   0,   2,                               // S
	  1,   0,  -2,   0,   0,  -3,   0,  -1,   0,   0,  -2,  -1,   0,   0,  -1,  -1,   1,   3,                          // T
	  0,  -2,  -2,  -2,  -2,  -1,  -1,  -2,   4,  -2,   2,   2,  -2,  -1,  -2,  -2,  -1,   0,   4,                     // V
	 -6,  -5,  -8,  -7,  -7,   0,  -7,  -3,  -5,  -3,  -2,  -4,  -4,  -6,  -5,   2,  -2,  -5,  -6,  17,                // W
	 -3,  -3,   0,  -4,  -4,   7,  -5,   0,  -1,  -4,  -1,  -2,  -2,  -5,  -4,  -4,  -3,  -3,  -2,   0,  10,           // Y
	  0,   2,  -5,   3,   3,  -5,   0,   2,  -2,   0,  -3,  -2,   1,   0,   3,   0,   0,  -1,  -2,  -6,  -4,   3,      // Z
	  0,  -1,  -3,  -1,  -1,  -2,  -1,  -1,  -1,  -1,  -1,  -1,   0,  -1,  -1,  -1,   0,   0,  -1,  -4,  -2,  -1,  -1, // x
};

const int8 kMPam30[] = {
	  6,                                                                                                               // A
	 -3,   6,                                                                                                          // B
	 -6, -12,  10,                                                                                                     // C
	 -3,   6, -14,   8,                                                                                                // D
	 -2,   1, -14,   2,   8,                                                                                           // E
	 -8, -10, -13, -15, -14,   9,                                                                                      // F
	 -2,  -3,  -9,  -3,  -4,  -9,   6,                                                                                 // G
	 -7,  -1,  -7,  -4,  -5,  -6,  -9,   9,                                                                            // H
	 -5,  -6,  -6,  -7,  -5,  -2, -11,  -9,   8,                                                                       // I
	 -7,  -2, -14,  -4,  -4, -14,  -7,  -6,  -6,   7,                                                                  // K
	 -6,  -9, -15, -12,  -9,  -3, -10,  -6,  -1,  -8,   7,                                                             // L
	 -5, -10, -13, -11,  -7,  -4,  -8, -10,  -1,  -2,   1,  11,                                                        // M
	 -4,   6, -11,   2,  -2,  -9,  -3,   0,  -5,  -1,  -7,  -9,   8,                                                   // N
	 -2,  -7,  -8,  -8,  -5, -10,  -6,  -4,  -8,  -6,  -7,  -8,  -6,   8,                                              // P
	 -4,  -3, -14,  -2,   1, -13,  -7,   1,  -8,  -3,  -5,  -4,  -3,  -3,   8,                                         // Q
	 -7,  -7,  -8, -10,  -9,  -9,  -9,  -2,  -5,   0,  -8,  -4,  -6,  -4,  -2,   8,                                    // R
	  0,  -1,  -3,  -4,  -4,  -6,  -2,  -6,  -7,  -4,  -8,  -5,   0,  -2,  -5,  -3,   6,                               // S
	 -1,  -3,  -8,  -5,  -6,  -9,  -6,  -7,  -2,  -3,  -7,  -4,  -2,  -4,  -5,  -6,   0,   7,                          // T
	 -2,  -8,  -6,  -8,  -6,  -8,  -5,  -6,   2,  -9,  -2,  -1,  -8,  -6,  -7,  -8,  -6,  -3,   7,                     // V
	-13, -10, -15, -15, -17,  -4, -15,  -7, -14, -12,  -6, -13,  -8, -14, -13,  -2,  -5, -13, -15,  13,                // W
	 -8,  -6,  -4, -11,  -8,   2, -14,  -3,  -6,  -9,  -7, -11,  -4, -13, -12, -10,  -7,  -6,  -7,  -5,  10,           // Y
	 -3,   0, -14,   1,   6, -13,  -5,  -1,  -6,  -4,  -7,  -5,  -3,  -4,   6,  -4,  -5,  -6,  -6, -14,  -9,   6,      // Z
	 -3,  -5,  -9,  -5,  -5,  -8,  -5,  -5,  -5,  -5,  -6,  -5,  -3,  -5,  -5,  -6,  -3,  -4,  -5, -11,  -7,  -5,  -5, // x
};

const int8 kMPam70[] = {
	  5,                                                                                                               // A
	 -1,   5,                                                                                                          // B
	 -4,  -8,   9,                                                                                                     // C
	 -1,   5,  -9,   6,                                                                                                // D
	 -1,   2,  -9,   3,   6,                                                                                           // E
	 -6,  -7,  -8, -10,  -9,   8,                                                                                      // F
	  0,  -1,  -6,  -1,  -2,  -7,   6,                                                                                 // G
	 -4,   0,  -5,  -1,  -2,  -4,  -6,   8,                                                                            // H
	 -2,  -4,  -4,  -5,  -4,   0,  -6,  -6,   7,                                                                       // I
	 -4,  -1,  -9,  -2,  -2,  -9,  -5,  -3,  -4,   6,                                                                  // K
	 -4,  -6, -10,  -8,  -6,  -1,  -7,  -4,   1,  -5,   6,                                                             // L
	 -3,  -6,  -9,  -7,  -4,  -2,  -6,  -6,   1,   0,   2,  10,                                                        // M
	 -2,   5,  -7,   3,   0,  -6,  -1,   1,  -3,   0,  -5,  -5,   6,                                                   // N
	  0,  -4,  -5,  -4,  -3,  -7,  -3,  -2,  -5,  -4,  -5,  -5,  -3,   7,                                              // P
	 -2,  -1,  -9,   0,   2,  -9,  -4,   2,  -5,  -1,  -3,  -2,  -1,  -1,   7,                                         // Q
	 -4,  -4,  -5,  -6,  -5,  -7,  -6,   0,  -3,   2,  -6,  -2,  -3,  -2,   0,   8,                                    // R
	  1,   0,  -1,  -1,  -2,  -4,   0,  -3,  -4,  -2,  -6,  -3,   1,   0,  -3,  -1,   5,                               // S
	  1,  -1,  -5,  -2,  -3,  -6,  -3,  -4,  -1,  -1,  -4,  -2,   0,  -2,  -3,  -4,   2,   6,                          // T
	 -1,  -5,  -4,  -5,  -4,  -5,  -3,  -4,   3,  -6,   0,   0,  -5,  -3,  -4,  -5,  -3,  -1,   6,                     // V
	 -9,  -7, -11, -10, -11,  -2, -10,  -5,  -9,  -7,  -4,  -8,  -6,  -9,  -8,   0,  -3,  -8, -10,  13,                // W
	 -5,  -4,  -2,  -7,  -6,   4,  -9,  -1,  -4,  -7,  -4,  -7,  -3,  -9,  -8,  -7,  -5,  -4,  -5,  -3,   9,           // Y
	 -1,   1,  -9,   2,   5,  -9,  -3,   1,  -4,  -2,  -4,  -3,  -1,  -2,   5,  -2,  -2,  -3,  -4, -10,  -7,   5,      // Z
	 -2,  -2,  -6,  -3,  -3,  -5,  -3,  -3,  -3,  -3,  -4,  -3,  -2,  -3,  -2,  -3,  -1,  -2,  -2,  -7,  -5,  -3,  -3, // x
};

const MatrixData kMatrixData[] = {
    { "BLOSUM45", 13, 3, kMBlosum45, { 0.207, 0.049, 0.14,  1.5,  -22 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 12, 3, kMBlosum45, { 0.199, 0.039, 0.11,  1.8,  -34 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 11, 3, kMBlosum45, { 0.190, 0.031, 0.095, 2.0,  -38 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 10, 3, kMBlosum45, { 0.179, 0.023, 0.075, 2.4,  -51 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 16, 2, kMBlosum45, { 0.210, 0.051, 0.14,  1.5,  -24 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 15, 2, kMBlosum45, { 0.203, 0.041, 0.12,  1.7,  -31 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 14, 2, kMBlosum45, { 0.195, 0.032, 0.10,  1.9,  -36 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 13, 2, kMBlosum45, { 0.185, 0.024, 0.084, 2.2,  -45 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 12, 2, kMBlosum45, { 0.171, 0.016, 0.061, 2.8,  -65 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 19, 1, kMBlosum45, { 0.205, 0.040, 0.11,  1.9,  -43 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 18, 1, kMBlosum45, { 0.198, 0.032, 0.10,  2.0,  -43 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 17, 1, kMBlosum45, { 0.189, 0.024, 0.079, 2.4,  -57 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM45", 16, 1, kMBlosum45, { 0.176, 0.016, 0.063, 2.8,  -67 }, { 0.2291, 0.0924, 0.2514, 0.9113, -5.7 } },
    { "BLOSUM50", 13, 3, kMBlosum50, { 0.212, 0.063, 0.19,  1.1,  -16 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 12, 3, kMBlosum50, { 0.206, 0.055, 0.17,  1.2,  -18 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 11, 3, kMBlosum50, { 0.197, 0.042, 0.14,  1.4,  -25 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 10, 3, kMBlosum50, { 0.186, 0.031, 0.11,  1.7,  -34 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50",  9, 3, kMBlosum50, { 0.172, 0.022, 0.082, 2.1,  -48 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 16, 2, kMBlosum50, { 0.215, 0.066, 0.20,  1.05, -15 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 15, 2, kMBlosum50, { 0.210, 0.058, 0.17,  1.2,  -20 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 14, 2, kMBlosum50, { 0.202, 0.045, 0.14,  1.4,  -27 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 13, 2, kMBlosum50, { 0.193, 0.035, 0.12,  1.6,  -32 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 12, 2, kMBlosum50, { 0.181, 0.025, 0.095, 1.9,  -41 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 19, 1, kMBlosum50, { 0.212, 0.057, 0.18,  1.2,  -21 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 18, 1, kMBlosum50, { 0.207, 0.050, 0.15,  1.4,  -28 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 17, 1, kMBlosum50, { 0.198, 0.037, 0.12,  1.6,  -33 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 16, 1, kMBlosum50, { 0.186, 0.025, 0.10,  1.9,  -42 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM50", 15, 1, kMBlosum50, { 0.171, 0.015, 0.063, 2.7,  -76 }, { 0.2318, 0.112, 0.3362, 0.6895, -4.0 } },
    { "BLOSUM62", 11, 2, kMBlosum62, { 0.297, 0.082, 0.27,  1.1,  -10 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62", 10, 2, kMBlosum62, { 0.291, 0.075, 0.23,  1.3,  -15 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62",  9, 2, kMBlosum62, { 0.279, 0.058, 0.19,  1.5,  -19 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62",  8, 2, kMBlosum62, { 0.264, 0.045, 0.15,  1.8,  -26 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62",  7, 2, kMBlosum62, { 0.239, 0.027, 0.10,  2.5,  -46 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62",  6, 2, kMBlosum62, { 0.201, 0.012, 0.061, 3.3,  -58 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62", 13, 1, kMBlosum62, { 0.292, 0.071, 0.23,  1.2,  -11 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62", 12, 1, kMBlosum62, { 0.283, 0.059, 0.19,  1.5,  -19 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62", 11, 1, kMBlosum62, { 0.267, 0.041, 0.14,  1.9,  -30 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62", 10, 1, kMBlosum62, { 0.243, 0.024, 0.10,  2.5,  -44 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM62",  9, 1, kMBlosum62, { 0.206, 0.010, 0.052, 4.0,  -87 }, { 0.3176, 0.134, 0.4012, 0.7916, -3.2 } },
    { "BLOSUM80", 25, 2, kMBlosum80, { 0.342, 0.17,  0.66,  0.52, -1.6}, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80", 13, 2, kMBlosum80, { 0.336, 0.15,  0.57,  0.59, -3  }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80",  9, 2, kMBlosum80, { 0.319, 0.11,  0.42,  0.76, -6  }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80",  8, 2, kMBlosum80, { 0.308, 0.090, 0.35,  0.89, -9  }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80",  7, 2, kMBlosum80, { 0.293, 0.070, 0.27,  1.1,  -14 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80",  6, 2, kMBlosum80, { 0.268, 0.045, 0.19,  1.4,  -19 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80", 11, 1, kMBlosum80, { 0.314, 0.095, 0.35,  0.90, -9  }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80", 10, 1, kMBlosum80, { 0.299, 0.071, 0.27,  1.1,  -14 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM80",  9, 1, kMBlosum80, { 0.279, 0.048, 0.20,  1.4,  -19 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM90",  9, 2, kMBlosum90, { 0.310, 0.12,  0.46,  0.67, -6  }, { 0.3346, 0.190, 0.7547, 0.4434, -1.4 } },
    { "BLOSUM90",  8, 2, kMBlosum90, { 0.300, 0.099, 0.39,  0.76, -7  }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM90",  7, 2, kMBlosum90, { 0.283, 0.072, 0.30,  0.93, -11 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM90",  6, 2, kMBlosum90, { 0.259, 0.048, 0.22,  1.2,  -16 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM90", 11, 1, kMBlosum90, { 0.302, 0.093, 0.39,  0.78, -8  }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM90", 10, 1, kMBlosum90, { 0.290, 0.075, 0.28,  1.04, -15 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "BLOSUM90",  9, 1, kMBlosum90, { 0.265, 0.044, 0.20,  1.3,  -19 }, { 0.3430, 0.177, 0.6568, 0.5222, -1.6 } },
    { "PAM250",   15, 3, kMPam250,   { 0.205, 0.049, 0.13,  1.6,  -23 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   14, 3, kMPam250,   { 0.200, 0.043, 0.12,  1.7,  -26 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   13, 3, kMPam250,   { 0.194, 0.036, 0.10,  1.9,  -31 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   12, 3, kMPam250,   { 0.186, 0.029, 0.085, 2.2,  -41 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   11, 3, kMPam250,   { 0.174, 0.020, 0.070, 2.5,  -48 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   17, 2, kMPam250,   { 0.204, 0.047, 0.12,  1.7,  -28 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   16, 2, kMPam250,   { 0.198, 0.038, 0.11,  1.8,  -29 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   15, 2, kMPam250,   { 0.191, 0.031, 0.087, 2.2,  -44 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   14, 2, kMPam250,   { 0.182, 0.024, 0.073, 2.5,  -53 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   13, 2, kMPam250,   { 0.171, 0.017, 0.059, 2.9,  -64 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   21, 1, kMPam250,   { 0.205, 0.045, 0.11,  1.8,  -34 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   20, 1, kMPam250,   { 0.199, 0.037, 0.10,  1.9,  -35 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   19, 1, kMPam250,   { 0.192, 0.029, 0.083, 2.3,  -52 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   18, 1, kMPam250,   { 0.183, 0.021, 0.070, 2.6,  -60 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM250",   17, 1, kMPam250,   { 0.171, 0.014, 0.052, 3.3,  -86 }, { 0.2252, 0.0868, 0.2223, 0.98, -5.0 } },
    { "PAM30",     7, 2, kMPam30,    { 0.305, 0.15,  0.87,  0.35, -3  }, { 0.3400, 0.283, 1.754, 0.1938, -0.3 } },
    { "PAM30",     6, 2, kMPam30,    { 0.287, 0.11,  0.68,  0.42, -4  }, { 0.3400, 0.283, 1.754, 0.1938, -0.3 } },
    { "PAM30",     5, 2, kMPam30,    { 0.264, 0.079, 0.45,  0.59, -7  }, { 0.3400, 0.283, 1.754, 0.1938, -0.3 } },
    { "PAM30",    10, 1, kMPam30,    { 0.309, 0.15,  0.88,  0.35, -3  }, { 0.3400, 0.283, 1.754, 0.1938, -0.3 } },
    { "PAM30",     9, 1, kMPam30,    { 0.294, 0.11,  0.61,  0.48, -6  }, { 0.3400, 0.283, 1.754, 0.1938, -0.3 } },
    { "PAM30",     8, 1, kMPam30,    { 0.270, 0.072, 0.40,  0.68, -10 }, { 0.3400, 0.283, 1.754, 0.1938, -0.3 } },
    { "PAM70",     8, 2, kMPam70,    { 0.301, 0.12,  0.54,  0.56, -5  }, { 0.3345, 0.229, 1.029, 0.3250, -0.7 } },
    { "PAM70",     7, 2, kMPam70,    { 0.286, 0.093, 0.43,  0.67, -7  }, { 0.3345, 0.229, 1.029, 0.3250, -0.7 } },
    { "PAM70",     6, 2, kMPam70,    { 0.264, 0.064, 0.29,  0.90, -12 }, { 0.3345, 0.229, 1.029, 0.3250, -0.7 } },
    { "PAM70",    11, 1, kMPam70,    { 0.305, 0.12,  0.52,  0.59, -6  }, { 0.3345, 0.229, 1.029, 0.3250, -0.7 } },
    { "PAM70",    10, 1, kMPam70,    { 0.291, 0.091, 0.41,  0.71, -9  }, { 0.3345, 0.229, 1.029, 0.3250, -0.7 } },
    { "PAM70",     9, 1, kMPam70,    { 0.270, 0.060, 0.28,  0.97, -14 }, { 0.3345, 0.229, 1.029, 0.3250, -0.7 } },
    {  }
};

const float
	kMPam250ScalingFactor = log(2.f) / 3.0f,
	kMPam250MisMatchAverage = -1.484210526f;
