// Import portion of Track to Route Converter - Trk2Rt_Import.cpp
// by Steve Follen, October 2023 - December 2025

#include "Trk2Rt.h"

// Maximum lengths, including the null terminator (number of wide charaters, not bytes)
constexpr int ROAD_ID_LEN = 15; // map segment plus road id part of subclass, without the last flag byte, plus a terminating null

constexpr int RND_FACTOR_6 = 1000000; // round lat and lon to 6 decimal places to prevent very near duplicate points

bool importedNum; // true if imported route included previous numbering - indicated by ": " in a route point name

// Determine the number of intermediate track points and road id changes, if any, between each pair of primary route points.
// For each primary route point, store the number of each immediately following that route point.
void t2rSetIntermediateCounts(void) {
    RoutePoint* rtPt;
    RoutePoint* rtPtNext = NULL;
    TrackPoint* trkPt;
    int rdIdChgCount = 0;

    trkPt = theTrack.getFirst();
    rtPt = prmRtPtList.getFirst();
    if (rtPt) {
        rtPtNext = prmRtPtList.getNext(rtPt);
    }

    while (trkPt && rtPt && rtPtNext) {
        
        if (rtPtNext->clsTrkPtIdx > rtPt->clsTrkPtIdx) {
            rtPt->nTrkPts = rtPtNext->clsTrkPtIdx - rtPt->clsTrkPtIdx - 1;
            if (trackSource == TS_ROUTE) {
                while (trkPt->getIdx() < rtPtNext->clsTrkPtIdx) {
                    if (trkPt->isRdIdChg()) {
                        rdIdChgCount++;
                    }
                    trkPt = theTrack.getNext(trkPt);
                }
                rtPt->nRdIdChg = rdIdChgCount;
                rdIdChgCount = 0;
            } // else there are no known road id changes
        }
        else {
            rtPt->nRdIdChg = rtPt->nTrkPts = 0;
        }

        if (trackSource == TS_ROUTE) {
            rtPt->nShpPts = (int)(rtPt->nRdIdChg * rtPt->shPtPC / 100);
        }
        else {
            rtPt->nShpPts = (int)(rtPt->nTrkPts * rtPt->shPtPC / 100);
        }

        rtPt = rtPtNext;
        rtPtNext = prmRtPtList.getNext(rtPt);
    }
}

// Haversine (great circle) approximation of distance between two points.
// Distance returned is in feet.
static double haversine(double lat1, double lon1, double lat2, double lon2)
{
    const double PI = 3.14159;

    // lat and lon differences (as radians)
    double dLat = (lat2 - lat1) * PI / 180.0;
    double dLon = (lon2 - lon1) * PI / 180.0;

    // convert lats to radians
    lat1 = lat1 * PI / 180.0;
    lat2 = lat2 * PI / 180.0;

    // return Haversine
    return (double)EARTH_RAD_FT * 2 * asin(sqrt(pow(sin(dLat / 2), 2) + cos(lat1) * cos(lat2) * pow(sin(dLon / 2), 2)));
}

// Identify the track point closest to each primary route point and
// save associated trackpoint data in the associated route point.
// Sort routepoints in track point order.
// Should be called only if primary route points and a track have been read from the import file.
static void t2rSetPrimaryRtPtsClosestTrkPt(void)
{
    double dist;    // Haversine distance between 2 points
    RoutePoint* rtPt; // route point
    TrackPoint* trkPt;
    int idx = 0;

    trkPt = theTrack.getFirst();
    while (trkPt) {
        rtPt = prmRtPtList.getFirst();
        while (rtPt) {
            dist = haversine(rtPt->lat, rtPt->lon, trkPt->lat, trkPt->lon);
            if (dist < rtPt->clsTrkPtDist) {
                rtPt->clsTrkPtDist = dist;
                rtPt->clsTrkPtIdx = trkPt->getIdx();
                rtPt->trkSeg = trkPt;
                rtPt->clsPt.lat = trkPt->lat;
                rtPt->clsPt.lon = trkPt->lon;
            }
            rtPt = prmRtPtList.getNext(rtPt);
        }
        trkPt = theTrack.getNext(trkPt);
        idx++;
    }
    prmRtPtList.reorderPerTrack();
}

// If there is no track, set each primary route point's closest track point index to in-order values.
// They are needed when restoring route points from the exclude list.
static void t2rSetFakeTrkPtIdx(void)
{
    RoutePoint* rtPt; // route point
    int idx = 0;

    rtPt = prmRtPtList.getFirst();
    while (rtPt) {
        rtPt->clsTrkPtIdx = idx;
        // rtPt->trkSeg remains NULL
        idx++;
        rtPt = prmRtPtList.getNext(rtPt);
    }
}

// Use the standard Windows file open dialog to get the import file name
LRESULT t2rGetImportFileName()
{
    HRESULT hr;
    IFileOpenDialog* fileOpenDlg = NULL;
    IShellItem* shellItem = NULL;
    PWSTR filePath = NULL;
    int retVal = -1; // failure

    COMDLG_FILTERSPEC inFltrSpec[] =
    {
        { L"", L"*.gpx;*.kml"},
        { L"", L"*.gpx"},
        { L"", L"*.kml"},
    };

    do {
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&fileOpenDlg));
        if (!SUCCEEDED(hr))
        {
            break;
        }

        hr = fileOpenDlg->SetFileTypes(3, inFltrSpec);
        if (!SUCCEEDED(hr))
        {
            break;
        }

        hr = fileOpenDlg->SetFileTypeIndex(1);
        if (!SUCCEEDED(hr))
        {
            break;
        }

        hr = fileOpenDlg->SetDefaultExtension(L"gpx;kml");
        if (!SUCCEEDED(hr))
        {
            break;
        }

        hr = fileOpenDlg->Show(NULL);
        if (!SUCCEEDED(hr))
        {
            break;
        }

        hr = fileOpenDlg->GetResult(&shellItem);
        if (!SUCCEEDED(hr))
        {
            break;
        }

        hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
        if (!SUCCEEDED(hr))
        {
            break;
        }
        retVal = 0; //success
    } while (0);

    if (filePath != NULL) {
        if (lstrlenW(filePath) < MAX_IN_FILE_NAME_LEN) {
            wcscpy_s(importPathname, MAX_PATH, filePath);
        }
        else {
            wstring t2rMsg = L"Import path file is too long.";
            MessageBoxW(NULL, t2rMsg.c_str(), L"Error While Importing", MB_OK | MB_ICONERROR);
            retVal = -1;
        }
        CoTaskMemFree(filePath);
        filePath = NULL;
    }
    if (shellItem != NULL) {
        shellItem->Release();
        shellItem = NULL;
    }
    if (fileOpenDlg != NULL) {
        fileOpenDlg->Release();
        fileOpenDlg = NULL;
    }

    return retVal;
}

// route point comments are sometimes multi-line
// convert any carriage return - linefeed to a space
void CRLF2Space(char* str)
{
    char* readPtr;
    char* writePtr;

    if (str && strlen(str)) {
        readPtr = str;
        writePtr = str;
        while (*readPtr) {
            if (*readPtr == '\r' && *(readPtr + 1) == '\n') {
                *writePtr++ = ' ';
                readPtr += 2;
            }
            else {
                *writePtr++ = *readPtr++;
            }
        }
        *writePtr = '\0';
    }
}


// Parse a gpx import file, using rapidxml.
// Find the first route, if any, and store all of its route points (via and shaping) in prmRtPtList.
// If it is a Garmin style calculated route, with route point extensions,
// then also fill theTrack with all unique points and indicate which of those
// are subclassed points since those identify road id changes which fully define the route.
static void t2rGetRoute_GPX(xml_node<>* gpxNode)
{
    xml_node<>* rtNode;      // rte
    xml_node<>* rtNmNode;    // rte name
    xml_node<>* rtPtNode;    // rtept
    xml_node<>* rtPtNodeNext; // next rtept
    xml_node<>* rtPtNmNode;  // name
    xml_node<>* rtPtCmtNode; // cmt
    xml_node<>* extNode;     // extensions
    xml_node<>* shPtNode;    // trp:ShapingPoint
    xml_node<>* rtPtExtNode; // gpxx:RoutePointExtension
    xml_node<>* gpxRtPtNode; // gpxx:rpt
    xml_node<>* subClsNode;  // gpxx:Subclass
    xml_attribute<>* atrbLat; // attribute latitude
    xml_attribute<>* atrbLon; // attribute longitutde

    char* name;
    char* cmt; // comment
    char* subclass;
    char rdId[ROAD_ID_LEN];     // 8 byte map segment and road id from gpxx:Subclass, in hexadecimal, as char, null terminated
    char prevRdId[ROAD_ID_LEN]; // map segment and road id from the previous unique gpxx:Subclass
    double lat, lon;      // latitude and longitude
    double prevLat = 0.0; // previous latitude
    double prevLon = 0.0; // previous longitude
    wchar_t ptName[MAX_NAME_LEN];
    bool rdIdChange;       // true for a road Id change
    bool prevRdIdChange;   // true for previous orad Id change
    bool firstRtPt = true; // true for first route point
    RoutePoint* newRtPt; // new route point
    TrackPoint* trkPt;   // track point

    rtNode = gpxNode->first_node("rte");
    if (rtNode) {
        rtNmNode = rtNode->first_node("name");
        if (rtNmNode) {
            name = rtNmNode->value();
            mbstowcs_s(NULL, prmRtPtList.routeName, MAX_NAME_LEN, name, _TRUNCATE);
        }
        rtPtNode = rtNode->first_node("rtept");
    }
    else {
        rtPtNode = NULL;
    }

    while (rtPtNode) {
        rtPtNodeNext = rtPtNode->next_sibling("rtept");
        lat = lon = 0.0;
        prevRdId[0] = '\0';
        prevRdIdChange = false;
        name = cmt = NULL;
        atrbLat = rtPtNode->first_attribute("lat");
        atrbLon = rtPtNode->first_attribute("lon");
        rtPtNmNode = rtPtNode->first_node("name");
        rtPtCmtNode = rtPtNode->first_node("cmt");
        if (atrbLat && atrbLon && rtPtNmNode) {
            lat = strtod(atrbLat->value(), NULL);
            lon = strtod(atrbLon->value(), NULL);
            name = rtPtNmNode->value();
            lat = round(lat * RND_FACTOR_6) / RND_FACTOR_6;
            lon = round(lon * RND_FACTOR_6) / RND_FACTOR_6;
            if (rtPtCmtNode) {
                cmt = rtPtCmtNode->value();
                CRLF2Space(cmt);
                if ((strlen(cmt) > strlen(name)) && strstr(cmt, name)) {
                    name = cmt;
                }
            }
        }
        if ((lat != 0.0) && (lon != 0.0) && name) {
            if (strstr(name, ": ")) {
                importedNum = true;
            }
            mbstowcs_s(NULL, ptName, MAX_NAME_LEN, name, _TRUNCATE);
            newRtPt = prmRtPtList.appendLast(lat, lon, ptName);
            // The first route point is always part of the track.
            if (firstRtPt) {
                trkPt = theTrack.addTrackPoint(lat, lon, false);
                prevLat = lat;
                prevLon = lon;
                // need to get initial subclass below before setting firstRtPt = false;
            }
            // The last route point is also always part of the track, but it may have already been included from the preceeding gpxx:rpt
            if (!rtPtNodeNext && ((lat != prevLat) || (lon != prevLon))) {
                trkPt = theTrack.addTrackPoint(lat, lon, false);
            }
        }
        else {
            MessageBoxW(NULL, L"Invalid format while parsing route points", L"Error While Importing", MB_OK | MB_ICONERROR);
            break;
        }

        extNode = rtPtNode->first_node("extensions");
        if (extNode) {
            shPtNode = extNode->first_node("trp:ShapingPoint");
            if (shPtNode) {
                newRtPt->shPt = true;
            }
            rtPtExtNode = extNode->first_node("gpxx:RoutePointExtension");
            if (rtPtExtNode) {
                if (firstRtPt) {
                    // get the initial road id (which is not a change)
                    subClsNode = rtPtExtNode->first_node("gpxx:Subclass");
                    if (subClsNode) {
                        subclass = subClsNode->value();
                        strncpy_s(prevRdId, ROAD_ID_LEN, &subclass[4], ROAD_ID_LEN - 1);
                    }
                    firstRtPt = false;
                }
                gpxRtPtNode = rtPtExtNode->first_node("gpxx:rpt");
                // skip the first one - it is basically at the route point
                if (gpxRtPtNode) {
                    gpxRtPtNode = gpxRtPtNode->next_sibling("gpxx:rpt");
                }
            }
            else {
                gpxRtPtNode = NULL;
            }
            while (gpxRtPtNode) {
                trkPt = NULL;
                lat = lon = 0.0;
                rdIdChange = false;
                atrbLat = gpxRtPtNode->first_attribute("lat");
                atrbLon = gpxRtPtNode->first_attribute("lon");
                if (atrbLat && atrbLon) {
                    lat = strtod(atrbLat->value(), NULL);
                    lon = strtod(atrbLon->value(), NULL);
                    lat = round(lat * RND_FACTOR_6) / RND_FACTOR_6;
                    lon = round(lon * RND_FACTOR_6) / RND_FACTOR_6;
                }
                if ((lat != 0.0) && (lon != 0.0)) {
                    // a valid gpxx:rpt / track point, but it could be a duplicate (many are)
                }
                else {
                    MessageBoxW(NULL, L"Invalid xml format while parsing route point extensions", L"Error While Importing", MB_OK | MB_ICONERROR);
                    rtPtNodeNext = NULL; // break all the way out
                    break;
                }
                subClsNode = gpxRtPtNode->first_node("gpxx:Subclass");
                if (subClsNode) {
                    subclass = subClsNode->value();
                    strncpy_s(rdId, ROAD_ID_LEN, &subclass[4], ROAD_ID_LEN - 1);
                    // Not every subclass has a road id change.
                    if (strcmp(rdId, prevRdId) != 0) {
                        strncpy_s(prevRdId, ROAD_ID_LEN, rdId, ROAD_ID_LEN - 1);
                        rdIdChange = true;
                    }
                }
                if ((lat != prevLat) || lon != prevLon) {
                    // a unique gpxx:rpt / track point
                    trkPt = theTrack.addTrackPoint(lat, lon, rdIdChange);
                    prevLat = lat;
                    prevLon = lon;
                }
                else if (rdIdChange) {
                    // not a unique gpxx:rpt / track point, but it now has a changed road id
                    if (trkPt) { // always - this avoids a warning
                        trkPt->setRoadChange();
                    }
                }
                rdIdChange = false;
                gpxRtPtNode = gpxRtPtNode->next_sibling("gpxx:rpt");
            }
        }
        rtPtNode = rtPtNodeNext;
    }
    // Only one track (the first track in the file), if any, is supported.

    if (prmRtPtList.nPts()) {
        prmRtPtSource = PRPS_ROUTE;
    }
    if (theTrack.nPts() > prmRtPtList.nPts()) {
        trackSource = TS_ROUTE;
    }
    else {
        theTrack.empty();
    }
}

// Parse gpx import file, using rapidxml, to find all waypoints in the file,
// and store them in prmRtPtList. At import, all waypoints become primary route points,
// which are via points by default. Some may subsequently be changed to shaping points by the user.
// Called only if no route has been imported.
static void t2rGetWaypoints_GPX(xml_node<>* gpxNode)
{
    xml_node<>* wptnode;
    xml_node<>* wptname;
    xml_attribute<>* atrbLat;
    xml_attribute<>* atrbLon;
    char* name;
    double lat, lon;
    wchar_t ptName[MAX_NAME_LEN];

    wptnode = gpxNode->first_node("wpt");

    while (wptnode) {
        lat = lon = 0.0;
        name = NULL;
        atrbLat = wptnode->first_attribute("lat");
        atrbLon = wptnode->first_attribute("lon");
        wptname = wptnode->first_node("name");

        if (atrbLat && atrbLon && wptname) {
            lat = strtod(atrbLat->value(), NULL);
            lon = strtod(atrbLon->value(), NULL);
            name = wptname->value();
            lat = round(lat * RND_FACTOR_6) / RND_FACTOR_6;
            lon = round(lon * RND_FACTOR_6) / RND_FACTOR_6;
        }

        if ((lat != 0.0) && (lon != 0.0) && name) {
            mbstowcs_s(NULL, ptName, MAX_NAME_LEN, name, _TRUNCATE);
            prmRtPtList.appendLast(lat, lon, ptName);
        }
        else {
            MessageBoxW(NULL, L"Invalid format while parsing waypoints", L"Error While Importing", MB_OK | MB_ICONERROR);
            break;
        }
        wptnode = wptnode->next_sibling("wpt");
    }
    if (prmRtPtList.nPts()) {
        prmRtPtSource = PRPS_WAYPOINT;
    }
}

// Parse gpx import file, using rapidxml, to find all track points in the file,
// and store them in theTrack.
// Also save the track name, if any, as the track / route name.
// Called only if a calculated Garmin style route, with route point extensions, has not been imported.
static void t2rGetTrack_GPX(xml_node<>* gpxNode)
{
    xml_node<>* trknode = NULL;
    xml_node<>* trkname = NULL;
    xml_node<>* trkseq = NULL;
    xml_node<>* trkpt = NULL;
    xml_attribute<>* trkPtLat;
    xml_attribute<>* trkPtLon;
    char* name;
    double lat, lon;

    int trkPtCnt = 0; // track point count

    trknode = gpxNode->first_node("trk");
    if (trknode) {
        trkname = trknode->first_node("name");
        if (trkname) {
            name = trkname->value();
            mbstowcs_s(NULL, prmRtPtList.routeName, MAX_NAME_LEN, name, _TRUNCATE);
        }

        trkseq = trknode->first_node("trkseg");
    }
    if (trkseq) {
        trkpt = trkseq->first_node("trkpt");
    }

    while (trkpt) {
        lat = lon = 0.0;
        trkPtLat = trkpt->first_attribute("lat");
        trkPtLon = trkpt->first_attribute("lon");
        if (trkPtLat && trkPtLon) {
            lat = strtod(trkPtLat->value(), NULL);
            lon = strtod(trkPtLon->value(), NULL);
        }
        if ((lat != 0.0) && (lon != 0.0)) {
            theTrack.addTrackPoint(lat, lon);
        }
        else {
            MessageBoxW(NULL, L"Invalid XML format while parsing track points", L"Error While Importing", MB_OK | MB_ICONERROR);
            break;
        }

        trkpt = trkpt->next_sibling("trkpt");
    }
    // Only one track (the first track in the file), if any, is supported.
    if (theTrack.nPts()) {
        trackSource = TS_TRACK;
    }
}

// Parse kml import file, using rapidxml, to find all placemark points in the file,
// and store them in prmRtPtList.  At import all placemark points become via points by default.
static void t2rGetPlacemarkPts_KML(xml_node<>* docNode)
{
    xml_node<>* plmNode; // Placemark
    xml_node<>* ptNode;  // Point
    xml_node<>* nmNode;  // name
    xml_node<>* crdNode; // coordinates
    char* name;
    double lat, lon;
    wchar_t ptName[MAX_NAME_LEN];
    size_t len;

    plmNode = docNode->first_node("Placemark");

    while (plmNode) {
        lat = lon = 0.0;
        name = NULL;
        len = 0;

        ptNode = plmNode->first_node("Point");
        if (ptNode) {
            nmNode = plmNode->first_node("name");
            if (nmNode) {
                name = nmNode->value();
            }
            crdNode = ptNode->first_node("coordinates");
            if (crdNode) {
                string crdAsText = crdNode->value();
                istringstream triple(crdAsText);
                string lonStr, latStr, elvStr;
                if (getline(triple, lonStr, ',') &&
                    getline(triple, latStr, ',') &&
                    getline(triple, elvStr) && name)
                {
                    lat = stod(latStr);
                    lon = stod(lonStr);
                    // elevation not used 
                    len = strlen(name);
                }
            }
            if ((lat != 0.0) && (lon != 0.0) && (len > 0)) {
                mbstowcs_s(NULL, ptName, MAX_NAME_LEN, name, _TRUNCATE);
                prmRtPtList.appendLast(lat, lon, ptName);
            }
            else {
                MessageBoxW(NULL, L"Invalid xml format while parsing placemark points", L"Error While Importing", MB_OK | MB_ICONERROR);
                break;
            }
        }
        plmNode = plmNode->next_sibling("Placemark");
    }
    if (prmRtPtList.nPts()) {
        prmRtPtSource = PRPS_PMP;
    }
}

// Parse kml import file, using rapidxml, to find all linestring coordinates in the file,
// and store them in theTrack.
// Also save the LineString name, if any, as the track / route name.
static void t2rGetLinestring_KML(xml_node<>* docNode)
{
    xml_node<>* plmNode; // Placemark
    xml_node<>* lsNode = NULL;  // LineString
    xml_node<>* nmNode;  // name
    xml_node<>* crdNode; // coordinates
    char* name = NULL;
    double lat, lon;
    size_t len;
    int trkPtCnt = 0;

    plmNode = docNode->first_node("Placemark");

    while (plmNode) {
        lat = lon = 0.0;

        lsNode = plmNode->first_node("LineString");
        if (lsNode) {
            nmNode = plmNode->first_node("name");
            if (nmNode) {
                name = nmNode->value();
                len = strlen(name);
                mbstowcs_s(NULL, prmRtPtList.routeName, MAX_NAME_LEN, name, _TRUNCATE);
            }
            crdNode = lsNode->first_node("coordinates");
            if (crdNode) {
                string crdsStr = crdNode->value();
                istringstream crdsStrStream(crdsStr);
                string tripleStr;

                // The full set of linestring coordinates is now in the string stream.
                // Loop through crdsStrStream extracting until whitespace each time
                // which puts one set of coordinates at a time into tripleStr.
                // Add each lat - lon corrdinate pair (track point) to theTrack.
                // Continue until the end of crdsStrStream, which is the end of crdNode->value().
                while (crdsStrStream >> tripleStr) {
                    istringstream tripleStrStream(tripleStr);
                    string lonStr, latStr, elvStr;
                    lat = lon = 0.0;

                    if (getline(tripleStrStream, lonStr, ',') &&
                        getline(tripleStrStream, latStr, ',') &&
                        getline(tripleStrStream, elvStr))
                    {
                        lat = stod(latStr);
                        lon = stod(lonStr);
                        // elevation not used 
                    }
                    if ((lat != 0.0) && (lon != 0.0)) {
                        theTrack.addTrackPoint(lat, lon);
                    }
                    else {
                        MessageBoxW(NULL, L"Invalid XML format while parsing LineString coordinates", L"Error While Importing", MB_OK | MB_ICONERROR);
                        break;
                    }
                }
                break; // Only one linestring (the first in the file), if any, is supported.
            }
        }
        plmNode = plmNode->next_sibling("Placemark");
    }
    if (theTrack.nPts()) {
        trackSource = TS_LINESTRING;
    }
}

int t2rParseImportFile()
{
    int retVal = 0; // Return value
    wstring importInfoString;
    char impFl[MAX_PATH]; // import file
    wstring pctShPtLbl; // percent of points to addtional shaping points label
    size_t numChar; // number of charaters

    // Clear any previously imported data
    memset(ptNameBase, 0, sizeof(ptNameBase));
    prmRtPtList.empty();
    excPtList.empty();
    prmRtPtSource = PRPS_NO_PRP;
    theTrack.empty();
    trackSource = TS_NO_TRK;
    importedNum = false;  // set true if imported route points contain previous prefix and/or numbering

    // Clear any previous import result display
    if (hwndImportInfo) {
        SetWindowTextW(hwndImportInfo, L"");
    }
    if (hwndRouteName) {
        SetWindowTextW(hwndRouteName, L"");
    }
    if (hwndPtName) {
        SetWindowTextW(hwndPtName, L"");
    }
    importInfoString = L"";

    // reset any previous export selections
    expWpt = expTrk = expRt = true;
    if (hwndExpWpCheck) {
        EnableWindow(hwndExpWpCheck, true);
        Button_SetCheck(hwndExpWpCheck, BST_CHECKED);
    }
    if (hwndExpTrkCheck) {
        EnableWindow(hwndExpTrkCheck, true);
        Button_SetCheck(hwndExpTrkCheck, BST_CHECKED);
    }
    if (hwndExpRtCheck) {
        EnableWindow(hwndExpRtCheck, true);
        Button_SetCheck(hwndExpRtCheck, BST_CHECKED);
    }

    parseImFl = false; // True if there is an import file specified on the command line (or context menu start).
                       // Can only be true for an initial import file at startup, if an import file name was supplied.

    if (!runBkGrnd) { // can only run in the background from the command line
        // These windows could be created once (at WM_CREATE?), set to SW_HIDE here and
        // set to SW_NORMAL in t2rCreatePrmRtPtDetailWindows(void), rather than created there and destroyed here.
        // Doing so would affect the hwndDetailLbl check in t2rCheckAddViaPtsAtTrackEnds(). ... any other affects ?
        // Positioning would be handled by t2rPositionChildWindows(), which is called at the end of t2rCreatePrmRtPtDetailWindows(void), which is called from here.

        if (hwndDetailLbl) {
            DestroyWindow(hwndDetailLbl);
            hwndDetailLbl = NULL;
        }
        if (hwndShapeLbl) {
            DestroyWindow(hwndShapeLbl);
            hwndShapeLbl = NULL;
        }
        if (hwndExcludeLbl) {
            DestroyWindow(hwndExcludeLbl);
            hwndExcludeLbl = NULL;
        }
        if (hwndDistlLbl) {
            DestroyWindow(hwndDistlLbl);
            hwndDistlLbl = NULL;
        }
        if (hwndSplitlLbl) {
            DestroyWindow(hwndSplitlLbl);
            hwndSplitlLbl = NULL;
        }
        if (hwndDistUnits) {
            DestroyWindow(hwndDistUnits);
            hwndDistUnits = NULL;
        }
        if (hwndSplitButton) {
            DestroyWindow(hwndSplitButton);
            hwndSplitButton = NULL;
        }
        if (hwndExportButton) {
            DestroyWindow(hwndExportButton);
            hwndExportButton = NULL;
            DestroyWindow(hwndExportInfo);
            hwndExportInfo = NULL;
        }
        if (hwndExportInfo) {
            DestroyWindow(hwndExportInfo);
            hwndExportInfo = NULL;
        }
        if (hwndExpFileLbl) {
            DestroyWindow(hwndExpFileLbl);
            hwndExpFileLbl = NULL;
        }
        if (hwndExpFile) {
            DestroyWindow(hwndExpFile);
            hwndExpFile = NULL;
        }
        if (hwndExcSep) {
            DestroyWindow(hwndExcSep);
            hwndExcSep = NULL;
        }
        if (hwndExcListLbl) {
            DestroyWindow(hwndExcListLbl);
            hwndExcListLbl = NULL;
        }
        if (hwnd2ndExcLbl) {
            DestroyWindow(hwnd2ndExcLbl);
            hwnd2ndExcLbl = NULL;
        }
    }

    do {
        xml_document<>* doc = new xml_document<>();
        xml_node<>* gpxNode = NULL; // gpx node at top of doc, if import file is gpx
        xml_node<>* kmlNode = NULL; // kml node at top of doc, if import file is kml
        xml_node<>* docNode = NULL; // Document node inside kml node
        xml_node<>* fldNode = NULL; // possible Folder node(s) inside kml Document node
        xml_node<>* topNode = NULL; // Document or Folder node to start parsing placemark point and linestring corrdinates from

        if (!doc) {
            retVal = -1;
            break;
        }
        wcstombs_s(&numChar, impFl, MAX_PATH, importPathname, _TRUNCATE);

        // check file open so error can be handled if needed - avoid rapidxml throwing exception
        ifstream importFile;
        importFile.open(importPathname);
        if (!importFile.is_open()) {
            MessageBoxW(NULL, L"Could not open import file", L"Trk2Rt Import Aborted.", MB_OK | MB_ICONERROR);
            retVal = -1;
            break;
        }
        else {
            importFile.close();
        }

        file<> xmlFile(impFl);

        doc->parse<0>(xmlFile.data());  // parse the import file as xml

        gpxNode = doc->first_node("gpx");
        kmlNode = doc->first_node("kml");

        if (gpxNode) {
            t2rGetRoute_GPX(gpxNode);
            if (prmRtPtSource == PRPS_NO_PRP) {
                // No primary route points from a route in the import file, so try to find waypoints
                t2rGetWaypoints_GPX(gpxNode);
            }
            if (trackSource == TS_NO_TRK) {
                // No track from hidden route points from a calculated route in the import file, so try to find a track
                t2rGetTrack_GPX(gpxNode);
            }
        }
        else if (kmlNode) {
            docNode = kmlNode->first_node("Document");
            if (docNode) {
                topNode = docNode;
                // points and linestring may be inside one or more nested Folder nodes.
                fldNode = docNode->first_node("Folder");
                while (fldNode) {
                    topNode = fldNode;
                    fldNode = fldNode->first_node("Folder");
                }
                t2rGetPlacemarkPts_KML(topNode);
                t2rGetLinestring_KML(topNode);
            }
            else {
                MessageBoxW(NULL, L"Import file xml - kml format is invalid.", L"Trk2Rt Import Aborted.", MB_OK | MB_ICONERROR);
                retVal = -1;
                break;
            }
        }
        else {
            MessageBoxW(NULL, L"Import file is not valid xml format of gpx or kml.", L"Trk2Rt Import Aborted.", MB_OK | MB_ICONERROR);
            retVal = -1;
            break;
        }

        // Import file parsing completed
        doc->clear();
        delete doc;

        wcsncpy_s(ptNameBase, MAX_BASE_NAME_LEN, prmRtPtList.routeName, _TRUNCATE);

        if (trackSource == TS_ROUTE) {
            nShPtVal = nShPtValRt;
        }
        else {
            nShPtVal = nShPtValTrk;
        }
        nShPtIdx = 1;
        if (!runBkGrnd || (shPtPC > 101.0)) {  // wWinMain() sets shPtPC to 200 if running background and exportPercent= is not set there
            shPtPC = nShPtVal[nShPtIdx];
        }

        if ((theTrack.nPts() < 2) && (prmRtPtList.nPts() < 2)) {
            expRt = false;
            MessageBoxW(NULL, L"Import file contains nothing to export.", L"Trk2Rt Import Empty", MB_OK | MB_ICONWARNING);
        }
        else if ((theTrack.nPts() >= 2)) {
            t2rSetPrimaryRtPtsClosestTrkPt();
            t2rSetIntermediateCounts();
        }
        else {
            // there are only route points, no track
            t2rSetFakeTrkPtIdx();
        }

        if (theTrack.nPts() < 2) {
            expTrk = false;
        }
        else {
            t2rCheckAddViaPtsAtTrackEnds();
        }

        if (prmRtPtList.nPts() < 2) {
            expWpt = false;
        }

        if (!runBkGrnd) {
            if (hwndOpeningLbl) {
                DestroyWindow(hwndOpeningLbl);
                hwndOpeningLbl = NULL;
            }

            importInfoString = L"Imported ";

            switch (prmRtPtSource) {
            case PRPS_NO_PRP:
                importInfoString += L"no routepoints, nor waypoints and ";
                break;
            case PRPS_ROUTE:
                importInfoString += to_wstring(prmRtPtList.nPts()) + L" primary route points and ";
                break;
            case PRPS_WAYPOINT:
                importInfoString += to_wstring(prmRtPtList.nPts()) + L" waypoints and ";
                break;
            case PRPS_PMP:
                importInfoString += to_wstring(prmRtPtList.nPts()) + L" placemark points and ";
                break;
            }

            switch (trackSource) {
            case TS_NO_TRK:
                importInfoString += L"no additional route points, nor track";
                pctShPtLbl = L"";
                break;
            case TS_ROUTE:
                importInfoString += to_wstring(theTrack.nPts() - prmRtPtList.nPts()) + L" additional route points with " + to_wstring(theTrack.nRdChg()) + L" road changes.";
                pctShPtLbl = L" of road changes to additional route shaping points";
                break;
            case TS_TRACK:
                importInfoString += to_wstring(theTrack.nPts()) + L" track points";
                pctShPtLbl = L" of track points to additional route shaping points";
                break;
            case TS_LINESTRING:
                importInfoString += to_wstring(theTrack.nPts()) + L" linestring coordinates";
                pctShPtLbl = L" of linestring coordinates to additional route shaping points";
                break;
            }
            if (hwndImportInfo && hwndRouteName && hwndPtName) { // warning avoidance
                SetWindowTextW(hwndImportInfo, importInfoString.c_str());
                ShowWindow(hwndImportInfo, SW_SHOWNORMAL);
                ShowWindow(hwndUpSep, SW_SHOWNORMAL);
                if (expRt) {
                    if (!expWpt) {
                        Button_SetCheck(hwndExpWpCheck, BST_UNCHECKED);
                        EnableWindow(hwndExpWpCheck, false);
                    }
                    if (!expTrk) {
                        Button_SetCheck(hwndExpTrkCheck, BST_UNCHECKED);
                        EnableWindow(hwndExpTrkCheck, false);
                    }

                    SendMessage(hwndNumShPt, CB_RESETCONTENT, 0, 0); // clear content in case this is not the first file import
                    for (int k = 0; k <= 5; k += 1)
                    {
                        if (trackSource == TS_ROUTE) {
                            SendMessage(hwndNumShPt, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)nShPtTextRt[k]);// Add option strings to combobox
                        }
                        else {
                            SendMessage(hwndNumShPt, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)nShPtTextTrk[k]);// Add option strings to combobox
                        }
                    }
                    SendMessage(hwndNumShPt, CB_SETCURSEL, (WPARAM)1, (LPARAM)0); // Set & display initial selection

                    SetWindowTextW(hwndNumShPtLbl2, pctShPtLbl.c_str());

                    EnableWindow(hwndRouteName, true);
                    SetWindowTextW(hwndRouteName, prmRtPtList.routeName);
                    UpdateWindow(hwndRouteName);

                    EnableWindow(hwndPtName, true);
                    SetWindowTextW(hwndPtName, ptNameBase);
                    UpdateWindow(hwndPtName);

                    showGlobalConfig(true);

                    if (prmRtPtList.nPts()) {
                        t2rUpdateGlobalTrkPtPercent(true);
                        t2rCreatePrmRtPtDetailWindows();
                    }
                }
            }
        }
        else {
            t2rUpdateGlobalTrkPtPercent(false);
        }
    } while (0);

    return retVal;
}
