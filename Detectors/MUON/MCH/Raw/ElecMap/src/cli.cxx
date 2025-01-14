// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <iostream>
#include <fmt/format.h>
#include <boost/program_options.hpp>
#include "MCHRawElecMap/Mapper.h"
#include <stdexcept>
#include <fstream>

namespace po = boost::program_options;
using namespace o2::mch::raw;

template <typename ELECMAP>
int dump(const DsElecId& dsElecId,
         const DsDetId& dsDetId)
{
  auto solar2fee = createSolar2FeeLinkMapper<ELECMAP>();
  auto feelink = solar2fee(dsElecId.solarId());
  if (!feelink.has_value()) {
    std::cout << "Could not get FeeLinkId for solarId " << dsElecId.solarId() << "\n";
    return 4;
  }
  std::cout << fmt::format("{} (elinkId {:d}) [ {} ] {}\n",
                           asString(dsElecId),
                           dsElecId.elinkId(),
                           asString(feelink.value()),
                           asString(dsDetId));
  return 0;
}

template <typename ELECMAP>
int convertElec2Det(uint16_t solarId, uint8_t groupId, uint8_t indexId)
{
  try {
    std::cout << fmt::format("solarId {} groupId {} indexId {}\n",
                             solarId, groupId, indexId);
    DsElecId dsElecId(solarId, groupId, indexId);
    auto elec2det = createElec2DetMapper<ELECMAP>();
    auto dsDetId = elec2det(dsElecId);
    if (!dsDetId.has_value()) {
      std::cout << asString(dsElecId) << " is not (yet?) known to the electronic mapper\n";
      return 3;
    }
    return dump<ELECMAP>(dsElecId, dsDetId.value());
  } catch (const std::exception& e) {
    std::cout << e.what() << "\n";
    return 4;
  }
}

template <typename ELECMAP>
int convertDet2Elec(int deId, int dsId)
{
  try {
    DsDetId dsDetId(deId, dsId);
    auto det2elec = createDet2ElecMapper<ELECMAP>();
    auto dsElecId = det2elec(dsDetId);
    if (!dsElecId.has_value()) {
      std::cout << asString(dsDetId) << " is not (yet?) known to the electronic mapper\n";
      return 3;
    }
    return dump<ELECMAP>(dsElecId.value(), dsDetId);
  } catch (const std::exception& e) {
    std::cout << e.what() << "\n";
    return 5;
  }
}

static std::string readFileContent(std::string& filename)
{
  std::string content;
  std::string s;
  std::ifstream in(filename);
  while (std::getline(in, s)) {
    content += s;
    content += "\n";
  }
  return content;
};

void convertFeeLink2Solar(uint16_t feeId, uint8_t linkId, bool dummyElecMap,
                          std::string cruMap)
{
  FeeLink2SolarMapper feeLink2Solar;
  if (cruMap.size() > 0) {
    ElectronicMapperString::sCruMap = readFileContent(cruMap);
    feeLink2Solar = createFeeLink2SolarMapper<ElectronicMapperString>();
  } else if (dummyElecMap) {
    feeLink2Solar = createFeeLink2SolarMapper<ElectronicMapperDummy>();
  } else {
    feeLink2Solar = createFeeLink2SolarMapper<ElectronicMapperGenerated>();
  }
  FeeLinkId f(feeId, linkId);
  auto s = feeLink2Solar(f);
  if (!s.has_value()) {
    std::cout << asString(f) << " is not known to the mapping\n";
  } else {
    std::cout << asString(f) << " is handled by solar " << s.value() << "\n";
  }
}

void generateCruMap(bool dummyElecMap, std::string cruMap)
{
  std::set<uint16_t> solarUIDs;
  Solar2FeeLinkMapper solar2FeeLink;

  if (cruMap.size() > 0) {
    ElectronicMapperString::sCruMap = readFileContent(cruMap);
    solarUIDs = getSolarUIDs<ElectronicMapperString>();
    solar2FeeLink = createSolar2FeeLinkMapper<ElectronicMapperString>();
  } else if (dummyElecMap) {
    solarUIDs = getSolarUIDs<ElectronicMapperDummy>();
    solar2FeeLink = createSolar2FeeLinkMapper<ElectronicMapperDummy>();
  } else {
    solarUIDs = getSolarUIDs<ElectronicMapperGenerated>();
    solar2FeeLink = createSolar2FeeLinkMapper<ElectronicMapperGenerated>();
  }
  for (auto s : solarUIDs) {
    auto f = solar2FeeLink(s).value();
    std::cout << fmt::format("{:4d} {:4d} {:4d}\n", s, f.feeId(), f.linkId());
  }
}

int usage(const po::options_description& generic)
{
  std::cout << "This program converts MCH electronic mapping information to detector mapping information\n";
  std::cout << "(solarId,groupId,indexId)->(dsId,deId)";
  std::cout << "As well as the reverse operation\n";
  std::cout << generic << "\n";
  return 2;
}

int main(int argc, char** argv)
{
  int solarId;
  int groupId;
  int indexId;
  int deId;
  int dsId;
  int feeId;
  int linkId;
  bool dummyElecMap;
  bool doGenerateCruMap{false};

  po::variables_map vm;
  po::options_description generic("Generic options");

  // clang-format off
  generic.add_options()
      ("help,h", "produce help message")
      ("solarId,s",po::value<int>(&solarId),"solar id")
      ("groupId,g",po::value<int>(&groupId),"group id")
      ("indexId,i",po::value<int>(&indexId),"index id")
      ("dsId,d",po::value<int>(&dsId),"dual sampa id")
      ("deId,e",po::value<int>(&deId),"detection element id")
      ("feeId,f",po::value<int>(&feeId),"fee id")
      ("linkId,l",po::value<int>(&linkId),"link id")
      ("cru-map",po::value<std::string>()->default_value(""),"custom cru mapping from file")
      ("generate-cru-map",po::bool_switch(&doGenerateCruMap),"generate cru.map file")
      ("dummy-elecmap",po::value<bool>(&dummyElecMap)->default_value(false),"use dummy electronic mapping (only for debug!)")
      ;
  // clang-format on

  po::options_description cmdline;
  cmdline.add(generic);

  po::store(po::command_line_parser(argc, argv).options(cmdline).run(), vm);

  if (vm.count("help")) {
    return usage(generic);
  }

  try {
    po::notify(vm);
  } catch (boost::program_options::error& e) {
    std::cout << "Error: " << e.what() << "\n";
    exit(1);
  }

  if (vm.count("solarId") && vm.count("groupId") && vm.count("indexId")) {
    if (dummyElecMap) {
      return convertElec2Det<ElectronicMapperDummy>(static_cast<uint16_t>(solarId),
                                                    static_cast<uint8_t>(groupId),
                                                    static_cast<uint8_t>(indexId));
    } else {
      return convertElec2Det<ElectronicMapperGenerated>(static_cast<uint16_t>(solarId),
                                                        static_cast<uint8_t>(groupId),
                                                        static_cast<uint8_t>(indexId));
    }
  } else if (vm.count("deId") && vm.count("dsId")) {
    if (dummyElecMap) {
      return convertDet2Elec<ElectronicMapperDummy>(deId, dsId);
    } else {
      return convertDet2Elec<ElectronicMapperGenerated>(deId, dsId);
    }
  } else if (vm.count("feeId") && vm.count("linkId")) {
    convertFeeLink2Solar(static_cast<uint16_t>(feeId),
                         static_cast<uint8_t>(linkId), dummyElecMap,
                         vm["cru-map"].as<std::string>());

  } else if (doGenerateCruMap) {
    generateCruMap(dummyElecMap,
                   vm["cru-map"].as<std::string>());
  } else {
    std::cout << "Incorrect mix of options\n";
    return usage(generic);
  }

  return 0;
}
