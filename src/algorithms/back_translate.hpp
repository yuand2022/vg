#ifndef VG_ALGORITHMS_BACK_TRANSLATE_HPP_INCLUDED
#define VG_ALGORITHMS_BACK_TRANSLATE_HPP_INCLUDED

/**
 * \file back_translate.hpp
 *
 * Defines an algorithm for translating an Alignment from node ID space to named segment space. 
 */

#include "../handle.hpp"
#include <vg/vg.pb.h>

namespace vg {
namespace algorithms {
using namespace std;

/**
 * Translate the given Path in place from node ID space to named segment space.
 */
void back_translate_in_place(const NamedNodeBackTranslation* translation, Path& path);

}
}

#endif