/*

Copyright (c) 2013, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef GPX_DESCRIPTOR_H
#define GPX_DESCRIPTOR_H

#include "BaseDescriptor.h"

template <class DataFacadeT> class GPXDescriptor : public BaseDescriptor<DataFacadeT>
{
  private:
    DescriptorConfig config;
    FixedPointCoordinate current;
    DataFacadeT * facade;


    void AddRoutePoint(const FixedPointCoordinate &coordinate, std::vector<char> &output)
    {
        auto append = [&output](const std::string &txt) {
          output.insert(output.end(), txt.begin(), txt.end());
        };

        std::string tmp;

        append("<rtept");
        FixedPointCoordinate::convertInternalLatLonToString(coordinate.lat, tmp);
        append(" lat=\"");
        append(tmp);
        output.push_back('\"');

        FixedPointCoordinate::convertInternalLatLonToString(coordinate.lon, tmp);
        append(" lon=\"");
        append(tmp);
        append("\">");

        if (config.elevation) {
            FixedPointCoordinate::convertInternalElevationToString(coordinate.getEle(), tmp);
            append("<ele>");
            append(tmp);
            append("</ele>");
        }
        append("</rtept>");
    }


  public:
    GPXDescriptor(DataFacadeT *facade) : facade(facade) {}

    void SetConfig(const DescriptorConfig &c) { config = c; }

    // TODO: reorder parameters
    void Run(const RawRouteData &raw_route, http::Reply &reply)
    {
        std::string header("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<gpx creator=\"OSRM Routing Engine\" version=\"1.1\" "
                           "xmlns=\"http://www.topografix.com/GPX/1/1\" "
                           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                           "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 gpx.xsd"
                           "\">"
                           "<metadata><copyright author=\"FIXME\"><license>Data (c)"
                           " FIXME</license></copyright>"
                           "</metadata>"
                           "<rte>");
        reply.content.insert(reply.content.end(), header.begin(), header.end());
        const bool found_route = (raw_route.shortest_path_length != INVALID_EDGE_WEIGHT) &&
                                 (!raw_route.unpacked_path_segments.front().empty());
        if (found_route)
        {
            const auto &coordinates = raw_route.segment_end_coordinates;
            AddRoutePoint(coordinates.front().source_phantom.location, reply.content);

            for (const std::vector<PathData> &path_data_vector : raw_route.unpacked_path_segments)
            {
                for (const PathData &path_data : path_data_vector)
                {
                    const FixedPointCoordinate coordinate = facade->GetCoordinateOfNode(path_data.node);
                    AddRoutePoint(coordinate, reply.content);
                }
            }
            AddRoutePoint(coordinates.back().target_phantom.location, reply.content);

        }
        std::string footer("</rte></gpx>");
        reply.content.insert(reply.content.end(), footer.begin(), footer.end());
    }
};
#endif // GPX_DESCRIPTOR_H
