/*
 * Copyright (C) 2018  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include "lodepng.h"

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <wels/codec_api.h>
#include <libyuv.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("rec")) || (0 == commandlineArguments.count("odvd")) ) {
        std::cerr << argv[0] << " extracts the content from a given .rec file using a provided .odvd message specification into separate .csv files." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --rec=<Recording from an OD4Session> --odvd=<ODVD Message Specification>" << std::endl;
        std::cerr << "Example: " << argv[0] << " --rec=myRecording.rec --odvd=myMessages.odvd" << std::endl;
        retCode = 1;
    } else {
        // Maps of container-ID & sender-stamp.
        std::map<std::string, std::string> mapOfFilenames;
        std::map<std::string, std::string> mapOfEntries;
        std::map<std::string, size_t> mapOfEntriesSizes;
        std::map<std::string, bool> mapOfFilenamesThatHaveBeenReset;

        cluon::MessageParser mp;
        std::pair<std::vector<cluon::MetaMessage>, cluon::MessageParser::MessageParserErrorCodes> messageParserResult;
        {
            std::ifstream fin(commandlineArguments["odvd"], std::ios::in|std::ios::binary);
            if (fin.good()) {
                std::string input(static_cast<std::stringstream const&>(std::stringstream() << fin.rdbuf()).str()); // NOLINT
                fin.close();
                messageParserResult = mp.parse(input);
                std::clog << "Found " << messageParserResult.first.size() << " messages." << std::endl;
            }
            else {
                std::cerr << "[rec2csv-png]: Message specification '" << commandlineArguments["odvd"] << "' not found." << std::endl;
                return retCode = 1;
            }
        }

        std::fstream fin(commandlineArguments["rec"], std::ios::in|std::ios::binary);
        if (fin.good()) {
            fin.close();

            auto fileWriter = [argv, &mapOfFilenames, &mapOfEntries, &mapOfEntriesSizes, &mapOfFilenamesThatHaveBeenReset](){
              for(auto entries : mapOfFilenames) {
                  std::cerr << argv[0] << " writing '" << entries.second << ".csv'...";
                  // Reset files on first access.
                  std::ios_base::openmode openMode = std::ios::out|std::ios::binary|(mapOfFilenamesThatHaveBeenReset.count(entries.second) == 0 ? std::ios::trunc : std::ios::app);
                  std::fstream fout(entries.second + ".csv", openMode);
                  if (fout.good() && mapOfEntries.count(entries.first)) {
                      const std::string tmp{mapOfEntries[entries.first]};
                      fout.write(tmp.c_str(), static_cast<std::streamsize>(tmp.size()));
                      // Reset memory.
                      mapOfEntries[entries.first] = "";
                      mapOfEntriesSizes[entries.first] = 0;
                  }
                  fout.close();
                  mapOfFilenamesThatHaveBeenReset[entries.second] = true;
                  std::cerr << " done." << std::endl;
              }
            };

            ISVCDecoder *decoder{nullptr};
            {
                WelsCreateDecoder(&decoder);
                if (0 != WelsCreateDecoder(&decoder) && (nullptr != decoder)) {
                    std::cerr << "[rec2csv-png]: Failed to create openh264 decoder." << std::endl;
                    return retCode;
                }
                int logLevel{WELS_LOG_QUIET};
                decoder->SetOption(DECODER_OPTION_TRACE_LEVEL, &logLevel);

                SDecodingParam decodingParam;
                {
                    memset(&decodingParam, 0, sizeof(SDecodingParam));
                    decodingParam.eEcActiveIdc = ERROR_CON_DISABLE;
                    decodingParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
                }
                if (cmResultSuccess != decoder->Initialize(&decodingParam)) {
                    std::cerr << "[rec2csv-png]: Failed to initialize openh264 decoder." << std::endl;
                    return retCode;
                }
            }

            vpx_codec_ctx_t codecVP8{};
            {
                memset(&codecVP8, 0, sizeof(codecVP8));
                vpx_codec_err_t result = vpx_codec_dec_init(&codecVP8, &vpx_codec_vp8_dx_algo, nullptr, 0);
                if (!result) {
                    std::clog << "[rec2csv-png]: Using " << vpx_codec_iface_name(&vpx_codec_vp8_dx_algo) << std::endl;
                }
                else {
                    std::cerr << "[rec2csv-png]: Failed to initialize decoder: " << vpx_codec_err_to_string(result) << std::endl;
                }
            }
            vpx_codec_ctx_t codecVP9{};
            {
                vpx_codec_err_t result = vpx_codec_dec_init(&codecVP9, &vpx_codec_vp9_dx_algo, nullptr, 0);
                if (!result) {
                    std::clog << "[rec2csv-png]: Using " << vpx_codec_iface_name(&vpx_codec_vp9_dx_algo) << std::endl;
                }
                else {
                    std::cerr << "[rec2csv-png]: Failed to initialize decoder: " << vpx_codec_err_to_string(result) << std::endl;
                }
            }

            std::map<int32_t, cluon::MetaMessage> scope;
            for (const auto &e : messageParserResult.first) { scope[e.messageIdentifier()] = e; }

            constexpr bool AUTOREWIND{false};
            constexpr bool THREADING{false};
            cluon::Player player(commandlineArguments["rec"], AUTOREWIND, THREADING);

            constexpr const size_t TEN_MB{10*1024*1024};
            uint32_t envelopeCounter{0};
            int32_t oldPercentage = -1;
            while (player.hasMoreData()) {
                auto next = player.getNextEnvelopeToBeReplayed();
                if (next.first) {
                    {
                        envelopeCounter++;
                        const int32_t percentage = static_cast<int32_t>((static_cast<float>(envelopeCounter)*100.0f)/static_cast<float>(player.totalNumberOfEnvelopesInRecFile()));
                        if ( (percentage % 5 == 0) && (percentage != oldPercentage) ) {
                            std::cerr << "[rec2csv-png]: Processed " << percentage << "%." << std::endl;
                            oldPercentage = percentage;
                        }
                    }
                    cluon::data::Envelope env{std::move(next.second)};
                    if (scope.count(env.dataType()) > 0) {
                        cluon::FromProtoVisitor protoDecoder;
                        std::stringstream sstr(env.serializedData());
                        protoDecoder.decodeFrom(sstr);

                        cluon::MetaMessage m = scope[env.dataType()];
                        cluon::GenericMessage gm;
                        gm.createFrom(m, messageParserResult.first);
                        gm.accept(protoDecoder);

                        std::stringstream sstrKey;
                        sstrKey << env.dataType() << "/" << env.senderStamp();
                        const std::string KEY = sstrKey.str();

                        std::stringstream sstrFilename;
                        sstrFilename << m.messageName() << "-" << env.senderStamp();
                        const std::string __FILENAME = sstrFilename.str();
                        mapOfFilenames[KEY] = __FILENAME;

                        if (mapOfEntries.count(KEY) > 0) {
                            // Extract timestamps.
                            std::string timeStamps;
                            {
                                cluon::ToCSVVisitor csv(';', false, { {1,false}, {2,false}, {3,true}, {4,true}, {5,true}, {6,false} });
                                env.accept(csv);
                                timeStamps = csv.csv();
                            }

                            cluon::ToCSVVisitor csv(';', false);
                            gm.accept(csv);

                            std::string entry{stringtoolbox::split(timeStamps, '\n')[0] + csv.csv()};
                            mapOfEntries[KEY] += entry;
                            mapOfEntriesSizes[KEY] += entry.size();
                        }
                        else {
                            // Extract timestamps.
                            std::vector<std::string> timeStampsWithHeader;
                            {
                                // Skip senderStamp (as it is in file name) and serialzedData.
                                cluon::ToCSVVisitor csv(';', true, { {1,false}, {2,false}, {3,true}, {4,true}, {5,true}, {6,false} });
                                env.accept(csv);
                                timeStampsWithHeader = stringtoolbox::split(csv.csv(), '\n');
                            }

                            cluon::ToCSVVisitor csv(';', true);
                            gm.accept(csv);

                            std::vector<std::string> valuesWithHeader = stringtoolbox::split(csv.csv(), '\n');
                            std::string entry{timeStampsWithHeader.at(0) + valuesWithHeader.at(0) + '\n' + timeStampsWithHeader.at(1) + valuesWithHeader.at(1) + '\n'};
                            mapOfEntries[KEY] += entry;
                            mapOfEntriesSizes[KEY] += entry.size();
                        }

                        // Keep track of buffer sizes.
                        if (mapOfEntriesSizes[KEY] > TEN_MB) {
                            std::cerr << argv[0] << ": Buffer for '" << KEY << "' has consumed " << mapOfEntriesSizes[KEY] << "/" << TEN_MB << " bytes; dumping data to disk."<< std::endl;
                            fileWriter();
                        }

                        if (opendlv::proxy::ImageReading::ID() == env.dataType()) {
                            if (mkdir(__FILENAME.c_str(), 0755) != 0 && errno != EEXIST) {
                                std::cerr << "[rec2csv-png]: Failed to create directory '" << __FILENAME << "'" << std::endl;
                            }
                            else {
                                cluon::data::TimeStamp sampleTimeStamp{env.sampleTimeStamp()};
                                opendlv::proxy::ImageReading img = cluon::extractMessage<opendlv::proxy::ImageReading>(std::move(env));

                                std::string data{img.data()};
                                const uint32_t LEN{static_cast<uint32_t>(data.size())};

                                std::vector<unsigned char> image;
                                image.resize(img.width() * img.height() * 4);

                                vpx_codec_iter_t it{nullptr};
                                vpx_image_t *yuvFrame{nullptr};

                                bool gotFrame{true};
                                if ("h264" == img.fourcc()) {
                                    uint8_t* yuvData[3];
                                    SBufferInfo bufferInfo;
                                    memset(&bufferInfo, 0, sizeof (SBufferInfo));
                                    if (0 != decoder->DecodeFrame2(reinterpret_cast<const unsigned char*>(data.c_str()), LEN, yuvData, &bufferInfo)) {
                                        std::cerr << "[rec2csv-png]: H264 decoding for current frame failed." << std::endl;
                                        gotFrame = false;
                                    }
                                    else {
                                        if ( (gotFrame = (1 == bufferInfo.iBufferStatus)) ) {
                                            if (-1 == libyuv::I420ToABGR(yuvData[0], bufferInfo.UsrData.sSystemBuffer.iStride[0], yuvData[1], bufferInfo.UsrData.sSystemBuffer.iStride[1], yuvData[2], bufferInfo.UsrData.sSystemBuffer.iStride[1], image.data(), img.width() * 4, img.width(), img.height())) {
                                                std::cerr << "[rec2csv-png]: Error transforming color space." << std::endl;
                                                gotFrame = false;
                                            }
                                        }
                                    }
                                }
                                else if ("VP80" == img.fourcc()) {
                                    if (vpx_codec_decode(&codecVP8, reinterpret_cast<const unsigned char*>(data.c_str()), LEN, nullptr, 0)) {
                                        std::cerr << "[rec2csv-png]: Decoding for current frame failed." << std::endl;
                                    }
                                    else {
                                        while (nullptr != (yuvFrame = vpx_codec_get_frame(&codecVP8, &it))) {
                                            if (-1 == libyuv::I420ToABGR(yuvFrame->planes[VPX_PLANE_Y], yuvFrame->stride[VPX_PLANE_Y], yuvFrame->planes[VPX_PLANE_U], yuvFrame->stride[VPX_PLANE_U], yuvFrame->planes[VPX_PLANE_V], yuvFrame->stride[VPX_PLANE_V], image.data(), img.width() * 4, img.width(), img.height()) ) {
                                                std::cerr << "[rec2csv-png]: Error transforming color space." << std::endl;
                                                gotFrame = false;
                                            }
                                        }
                                    }
                                }
                                else if ("VP90" == img.fourcc()) {
                                    if (vpx_codec_decode(&codecVP9, reinterpret_cast<const unsigned char*>(data.c_str()), LEN, nullptr, 0)) {
                                        std::cerr << "[rec2csv-png]: Decoding for current frame failed." << std::endl;
                                    }
                                    else {
                                        while (nullptr != (yuvFrame = vpx_codec_get_frame(&codecVP9, &it))) {
                                            if (-1 == libyuv::I420ToABGR(yuvFrame->planes[VPX_PLANE_Y], yuvFrame->stride[VPX_PLANE_Y], yuvFrame->planes[VPX_PLANE_U], yuvFrame->stride[VPX_PLANE_U], yuvFrame->planes[VPX_PLANE_V], yuvFrame->stride[VPX_PLANE_V], image.data(), img.width() * 4, img.width(), img.height()) ) {
                                                std::cerr << "[rec2csv-png]: Error transforming color space." << std::endl;
                                                gotFrame = false;
                                            }
                                        }
                                    }
                                }

                                if (gotFrame) {
                                    std::stringstream tmp;
                                    tmp << __FILENAME << "/" << cluon::time::toMicroseconds(sampleTimeStamp) << ".png";
                                    const std::string str = tmp.str();
                                    auto r = lodepng::encode(str.c_str(), image, img.width(), img.height());
                                    if (r) {
                                        std::cerr << "[rec2csv-png]: lodePNG error " << r << ": "<< lodepng_error_text(r) << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // Clear buffer at the end.
            fileWriter();

            if (decoder) {
                decoder->Uninitialize();
                WelsDestroyDecoder(decoder);
            }

            vpx_codec_destroy(&codecVP8);
            vpx_codec_destroy(&codecVP9);
        }
        else {
            std::cerr << "[rec2csv-png]: Recording '" << commandlineArguments["rec"] << "' not found." << std::endl;
            retCode = 1;
        }
    }
    return retCode;
}

