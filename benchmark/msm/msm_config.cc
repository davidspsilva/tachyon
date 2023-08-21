#include "benchmark/msm/msm_config.h"

#include "tachyon/base/console/iostream.h"
#include "tachyon/base/flag/flag_parser.h"

namespace tachyon {

bool ECConfig::Parse(int argc, char** argv) {
  base::FlagParser parser;
  parser.AddFlag<base::Flag<std::vector<uint64_t>>>(&degrees_)
      .set_short_name("-n")
      .set_required()
      .set_help("The log number of points to test");
  {
    std::string error;
    if (!parser.Parse(argc, argv, &error)) {
      tachyon_cerr << error << std::endl;
      return false;
    }
  }

  base::ranges::sort(degrees_);
  return true;
}

std::vector<uint64_t> ECConfig::GetPointNums() const {
  std::vector<uint64_t> point_nums;
  for (uint64_t degree : degrees_) {
    point_nums.push_back(1 << degree);
  }
  return point_nums;
}

}  // namespace tachyon
