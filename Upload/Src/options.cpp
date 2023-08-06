#include "options.hpp"

#include <ndn-cxx/interest.hpp>

namespace ndn {
namespace chunks {

Options::Options()
  : interestLifetime(ndn::DEFAULT_INTEREST_LIFETIME)
  , maxRetriesOnTimeoutOrNack(3)
  , mustBeFresh(false)
  , isVerbose(false)
{
}

} // namespace chunks
} // namespace ndn
