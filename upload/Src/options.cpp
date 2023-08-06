#include "options.hpp"

#include <ndn-cxx/interest.hpp>

namespace ndn {
namespace chunks {

Options::Options()
  //: interestLifetime(ndn::DEFAULT_INTEREST_LIFETIME)
  : interestLifetime(time::milliseconds(4000))   //4000
  , maxRetriesOnTimeoutOrNack(20)   //3?  20
  , mustBeFresh(false)
  , isVerbose(false)
{
}

} // namespace chunks
} // namespace ndn
