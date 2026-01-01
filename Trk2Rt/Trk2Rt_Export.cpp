// Export portion of Track to Route Converter - Trk2Rt_Emport.cpp
// by Steve Follen, October 2023 - December 2025

#include "Trk2Rt.h"

// Maximum lengths, including the null terminator (number of wide charaters, not bytes)
constexpr int MAX_NAME_END_LEN = 13;
constexpr int MAX_RTPN_LEN = 17;

// Write the route beginning at pt to the export file gpxFileOut.
// pt must be valid.
// Return pointer to the start of the next route in prmRtPtList, if any, otherwise NULL
static RoutePoint* t2rexportRoute(wofstream& gpxFileOut, int rtPartNum, RoutePoint* pt)
{
    wstring rteStart = L"  <rte>\n"
        "    <name>";
    wstring rte2 = L"</name>\n"
        "    <extensions>\n"
        "      <trp:Trip>\n"
        "        <trp:TransportationMode>Motorcycling</trp:TransportationMode>\n"
        "      </trp:Trip>\n"
        "    </extensions>\n";
    wstring rteptStart = L"    <rtept lat=\"";
    wstring rtept2 = L"\" lon=\"";
    wstring rtept3 = L"\">\n"
        "      <name>";
    wstring rtept4 = L"</name>\n"
        "      <extensions>\n";
    wstring viaPtstr = L"        <trp:ViaPoint />\n";
    wstring shpPtstr = L"        <trp:ShapingPoint />\n";
    wstring rpExt1 = L"        <gpxx:RoutePointExtension>\n";
    wstring subCls = L"          <gpxx:Subclass>000000000000FFFFFFFFFFFFFFFFFFFFFFFF</gpxx:Subclass>\n";
    wstring rpExt2 = L"        </gpxx:RoutePointExtension>\n";
    wstring rteptEnd = L"      </extensions>\n"
        "    </rtept>\n";
    wstring rteEnd = L"  </rte>\n";

    wchar_t rtPN[MAX_RTPN_LEN];
    wchar_t ptPrefix[MAX_PREFIX_LEN] = L"";
    wchar_t viaPtName[MAX_NAME_LEN];
    wchar_t ptNameLead[MAX_NAME_LEAD] = L"";
    wchar_t ptStNumTxt[MAX_START_NUM_LEN] = L"";

    RoutePoint* nextPt;   // next primary route point
    static long expPtNum; // point numbering
    int expPtSubNum;      // shaping point sub-numbering
    int shpPtSpacing;     // additional shaping point spacing
    int ptSpacingCount;   // point count for additional shaping point spacing
    int expShPtCount;     // count of exported additional shaping points following pt
    TrackPoint* trkPt;    // track point

    if (rtPartNum < 2) {
        // this is the first or only route
        expPtNum = 0;
    }
    expPtSubNum = 1;

    gpxFileOut << rteStart;
    gpxFileOut << prmRtPtList.routeName;
    if (rtPartNum) {
        _snwprintf_s(rtPN, MAX_RTPN_LEN, _TRUNCATE, L" Part %d of %d", rtPartNum, prmRtPtList.getNumRoutes());
        gpxFileOut << rtPN;
    }
    gpxFileOut << rte2;

    do {
        nextPt = prmRtPtList.getNext(pt);
        if (!runBkGrnd) {
            if (pt->hwndPtPrfx) {
                GetWindowTextW(pt->hwndPtPrfx, ptPrefix, MAX_PREFIX_LEN);
            }

            if (pt->hwndPtStNum) {
                GetWindowTextW(pt->hwndPtStNum, ptStNumTxt, MAX_START_NUM_LEN);
                if (lstrlenW(ptStNumTxt)) {
                    expPtNum = wcstol(ptStNumTxt, NULL, 10);
                    expPtSubNum = 1;
                }
            }
        }

        gpxFileOut << rteptStart;
        if (reLocPrmRtPt && (pt->clsPt.lat != 0.0) && (pt->clsPt.lon != 0.0)) {
            gpxFileOut << pt->clsPt.lat;
            gpxFileOut << rtept2;
            gpxFileOut << pt->clsPt.lon;
        }
        else {
            gpxFileOut << pt->lat;
            gpxFileOut << rtept2;
            gpxFileOut << pt->lon;
        }
        gpxFileOut << rtept3;
        if (numStyIdx == NS_NONE) {
            if (wcslen(ptPrefix) > 0) {
                _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%s: ", ptPrefix);
            }
        }
        else if ((numStyIdx == NS_VIA) && pt->shPt) {
            // continuous numbering of via points with sub-numbering of shaping points, and this is a shaping point
            _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%s%02d.%d: ", ptPrefix, expPtNum, expPtSubNum);
            expPtSubNum++;
        }
        else {
            // continuous numbering of all points or not a shaping point (or both)
            _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%s%02d: ", ptPrefix, expPtNum);
        }
        gpxFileOut << ptNameLead;
        if (runBkGrnd) {
            gpxFileOut << pt->ptName;
        }
        else {
            GetWindowTextW(pt->hwndRtPtName, viaPtName, MAX_NAME_LEN);
            gpxFileOut << viaPtName;
        }
        gpxFileOut << rtept4;
        if (pt->shPt) {
            gpxFileOut << shpPtstr;
        }
        else {
            gpxFileOut << viaPtstr;
        }
        gpxFileOut << rteptEnd;

        if (pt->nShpPts) {
            trkPt = pt->trkSeg;
            if (pt->nRdIdChg) {
                shpPtSpacing = (int)(0.5 + (pt->nRdIdChg / (pt->nShpPts + 1)));
            }
            else {
                shpPtSpacing = (int)(0.5 + (pt->nTrkPts / (pt->nShpPts + 1)));
            }
            shpPtSpacing = max(1, shpPtSpacing);
            ptSpacingCount = 0;
            expShPtCount = 0;
            while (trkPt && (expShPtCount < pt->nShpPts) && (trkPt != nextPt->trkSeg)) {
                if (pt->nRdIdChg) {
                    if (trkPt->isRdIdChg()) {
                        ptSpacingCount++; // count only the road change points
                    }
                }
                else {
                    ptSpacingCount++; // count every track point
                }
                if (ptSpacingCount == shpPtSpacing) {
                    gpxFileOut << rteptStart;
                    gpxFileOut << trkPt->lat;
                    gpxFileOut << rtept2;
                    gpxFileOut << trkPt->lon;
                    gpxFileOut << rtept3;
                    gpxFileOut << ptPrefix;
                    if (numStyIdx == NS_ALL) { // continuous numbering of all points
                        expPtNum++;
                        gpxFileOut << setfill(L'0') << setw(2) << expPtNum << ": ";
                    }
                    else if (numStyIdx == NS_VIA) { // continuous waypoint numbering with shaping point sub-numbering
                        gpxFileOut << setfill(L'0') << setw(2) << expPtNum << "." << expPtSubNum << ": ";
                        expPtSubNum++;
                    }
                    else if ((wcslen(ptPrefix) > 0)) { // NS_NONE
                        gpxFileOut << ": ";
                    }
                    gpxFileOut << ptNameBase;
                    gpxFileOut << L" " << trkPt->getIdx();
                    gpxFileOut << rtept4;
                    gpxFileOut << shpPtstr;
                    gpxFileOut << rteptEnd;

                    ptSpacingCount = 0;
                    expShPtCount++;
                }
                trkPt = theTrack.getNext(trkPt);
            }
        }
        if (!nextPt || (pt->posInRt == RTPOS_LAST)) {
            break;
        }
        if (((numStyIdx == NS_ALL) || !nextPt->shPt))
        {
            expPtNum++;
            expPtSubNum = 1;
        }

        pt = nextPt;
    } while (1);

    gpxFileOut << rteEnd;
    gpxFileOut.flush();

    return nextPt;
}

// Export primary route points as waypoints, beginning with pt, until the end of the route
static RoutePoint* t2rExportWaypoints(wofstream& gpxFileOut, RoutePoint* pt)
{
    wstring wpt1 = L"  <wpt lat=\"";
    wstring wpt2 = L"\" lon=\"";
    wstring wpt3 = L"\">\n    <name>";
    wstring wpt4 = L"</name>\n  </wpt>\n";

    RoutePoint* nextPt = NULL;

    while (pt) {
        nextPt = prmRtPtList.getNext(pt);
        gpxFileOut << wpt1;
        gpxFileOut << pt->lat;
        gpxFileOut << wpt2;
        gpxFileOut << pt->lon;
        gpxFileOut << wpt3;
        gpxFileOut << pt->ptName;
        gpxFileOut << wpt4;
        if (pt->posInRt == RTPOS_LAST) {
            break;
        }
        pt = nextPt;
    }
    return nextPt;
}

// Export track points, beginning at trkPt, until the track point index reaches stopIdx, or the end of the track.
// Tthe track point with stopIdx is exported and that track point is returned so that the next route to be exported,
// if any, can begin with the same track point.
static TrackPoint* t2rExportTrack(wofstream& gpxFileOut, TrackPoint* trkPt, int stopIdx)
{
    wstring trk1 = L"  <trk>\n    <name>";
    wstring trk2 = L"</name>\n    <trkseg>\n";
    wstring tpt1 = L"      <trkpt lat=\"";
    wstring tpt2 = L"\" lon=\"";
    wstring tpt3 = L"\" />\n";
    wstring trk3 = L"    </trkseg>\n  </trk>\n";

    if (trkPt) {
        gpxFileOut << trk1;
        gpxFileOut << prmRtPtList.routeName;
        gpxFileOut << trk2;

        while (trkPt) {
            gpxFileOut << tpt1;
            gpxFileOut << trkPt->lat;
            gpxFileOut << tpt2;
            gpxFileOut << trkPt->lon;
            gpxFileOut << tpt3;
            if (trkPt->getIdx() >= stopIdx) {
                if (!theTrack.getNext(trkPt)) {
                    trkPt = NULL;
                }
                break;
            }
            trkPt = theTrack.getNext(trkPt);
        }
        gpxFileOut << trk3;
    }
    return trkPt;
}

// Export all waypoints, route(s) and / or track(s) per user selections and configuration
LRESULT t2rExport()
{
    wstring gpxHdr = L"<?xml version=\"1.0\" encoding=\"utf-8\"?><gpx creator=\"Trk2Rt\" version=\"1.1\"\n"
        "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\n"
        "http://www.garmin.com/xmlschemas/TripExtensions/v1 http://www.garmin.com/xmlschemas/TripExtensionsv1.xsd\n"
        "http://www.garmin.com/xmlschemas/ViaPointTransportationModeExtensions/v1 http://www.garmin.com/xmlschemas/ViaPointTransportationModeExtensionsv1.xsd\"\n"
        "xmlns=\"http://www.topografix.com/GPX/1/1\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
        "xmlns:trp=\"http://www.garmin.com/xmlschemas/TripExtensions/v1\"\n"
        "xmlns:vptm=\"http://www.garmin.com/xmlschemas/ViaPointTransportationModeExtensions/v1\">\n";
    wstring gpxEnd = L"</gpx>\n";


    int retVal = 0; // Return value
    wchar_t expFileNameBase[MAX_PATH];        // export file name base
    wchar_t expFileNameEnd[MAX_NAME_END_LEN]; // export file name end
    wchar_t exportFileName[MAX_PATH] = L"";   // complete export file name
    wstring  exportFilesString = L""; // string of all complete export file pathnames, for display
    wofstream gpxFileOut; // The export file
    wstring t2rMsg; // error or warning message test
    RoutePoint* wayPt = NULL;   // waypoint
    RoutePoint* routePt = NULL; // route point
    TrackPoint* trkPt = NULL;   //track point
    int trkPtIdx = INT_MAX; // track point index
    int  rtPartNum = 0; // route part number for split routes; remains 0 for single route
    bool startNewFile = true; // flag to start a new export gpx file. Each exported route gets its own file

    do {
        if (!(expWpt || expTrk || expRt)) {
            MessageBoxW(NULL, L"Nothing Selected to Export", L"Error While Exporting", MB_OK | MB_ICONERROR);
            retVal = -1;
            break;
        }
        if (!theTrack.nPts() && !prmRtPtList.nPts()) {
            MessageBoxW(NULL, L"Nothing Imported. Nothing to Export", L"Error While Exporting", MB_OK | MB_ICONERROR);
            retVal = -1;
            break;
        }
        if ((!prmRtPtList.nPts() || !expWpt) && (!theTrack.nPts() || !(expTrk || expRt))) {
            MessageBoxW(NULL, L"Nothing Imported to Support Selected Export", L"Error While Exporting", MB_OK | MB_ICONERROR);
            retVal = -1;
            break;
        }

        // prmRtPtList.routeName is initially read from the import file, but
        // is not updated when (if) it is changed in the edit window (hwndRouteName).
        // It is only needed when exporting the track or route.
        if (!runBkGrnd) {
            GetWindowTextW(hwndRouteName, prmRtPtList.routeName, MAX_NAME_LEN);
        }

        wchar_t* pFileName = wcsrchr(importFileName, L'\\') + 1;

        if (wcslen(expPath) > 0) {
            wcscpy_s(expFileNameBase, MAX_PATH, expPath); // copy the command line provided export path, if any
            wcscat_s(expFileNameBase, MAX_PATH, L"\\");   // add the trailing slash
        }
        else {
            wcsncpy_s(expFileNameBase, MAX_PATH, importFileName, lstrlenW(importFileName) - lstrlenW(pFileName)); // get the import file path including final backslash
            if (wcslen(expSubdir) > 0) {
                wcscat_s(expFileNameBase, MAX_PATH, expSubdir); // add the command line provided export directory name, if any
                wcscat_s(expFileNameBase, MAX_PATH, L"\\");     // add the trailing slash
            }
            else {
                wcscat_s(expFileNameBase, MAX_PATH, L"T2R\\"); // add the default export directory name
            }
        }
        CreateDirectoryW(expFileNameBase, NULL); // create the export directory; ERROR_ALREADY_EXISTS is harmless
        wcsncat_s(expFileNameBase, MAX_PATH, pFileName, lstrlenW(pFileName) - lstrlenW(wcsrchr(importFileName, L'.') + 1) - 1); // add original import file name without .extension

        if (expWpt) {
            wayPt = prmRtPtList.getFirst();
        }
        if (expRt) {
            routePt = prmRtPtList.getFirst();
        }
        if (expTrk) {
            trkPt = theTrack.getFirst();
        }

        if (prmRtPtList.getNumRoutes() > 1) {
            rtPartNum = 1;
        } // else rtPartNum reamins 0; there is only a single file to export; no route part numbers needed

        do {
            wcscpy_s(exportFileName, MAX_PATH, expFileNameBase);
            if (rtPartNum) {
                if (rtPartNum > 1) {
                    exportFilesString += L"\n";
                }
                _snwprintf_s(expFileNameEnd, MAX_NAME_END_LEN, _TRUNCATE, L"_T2R_P%02d.gpx", rtPartNum);
                wcsncat_s(exportFileName, MAX_PATH, expFileNameEnd, 12); // append export name and extension
            }
            else {
                wcsncat_s(exportFileName, MAX_PATH, L"_T2R.gpx", 8); // append export name and extension
            }
            exportFilesString += exportFileName;

            gpxFileOut.open(exportFileName);
            if (!(gpxFileOut.is_open())) {
                wstring tmpStr(exportFileName);
                t2rMsg = L"Failed to Open export file/n" + tmpStr;
                MessageBoxW(NULL, t2rMsg.c_str(), L"Error While Exporting", MB_OK | MB_ICONERROR);
                retVal = -1;
                break;
            }

            startNewFile = false;
            trkPtIdx = INT_MAX;

            gpxFileOut << gpxHdr;
            gpxFileOut << fixed << setprecision(6);  // point positions within ~111mm or 4.4 inches

            if (wayPt) {
                wayPt = t2rExportWaypoints(gpxFileOut, wayPt);
                if (wayPt && (wayPt->clsTrkPtIdx > 0)) {
                    trkPtIdx = wayPt->clsTrkPtIdx;
                }
            }

            if (routePt) {
                routePt = t2rexportRoute(gpxFileOut, rtPartNum, routePt);
                if (routePt && (routePt->clsTrkPtIdx > 0)) {
                    trkPtIdx = routePt->clsTrkPtIdx;
                }
            }
            // Note: if both waypoints and route are being exported, then wayPt == routePt here

            if (trkPt) {
                trkPt = t2rExportTrack(gpxFileOut, trkPt, trkPtIdx);
            }

            gpxFileOut << gpxEnd;
            gpxFileOut.close();
            // done exporting a route

            if (!wayPt && !routePt && !trkPt) {
                break; // done exporting
            } // else move on to the next route.
            
            rtPartNum++;
            if (rtPartNum > 99) { // sanity check
                retVal = -1;
                break;
            }
        } while (1);
    } while (0);

    if (!runBkGrnd) {
        SetWindowTextW(hwndExpFileLbl, L"Exported to:");
        SetWindowTextW(hwndExpFile, exportFilesString.c_str());
        ShowWindow(hwndExpFileLbl, SW_NORMAL);
        ShowWindow(hwndExpFile, SW_NORMAL);

        // If the export file name is too long for the current horizontal scroll range, then expand it.
        int expLen = (int)(lstrlenW(exportFileName) * cxChar * 1.1);
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE;
        GetScrollInfo(hwndMain, SB_HORZ, &si);
        if (expLen > si.nMax) {
            SetScrollRange(hwndMain, SB_HORZ, 0, expLen, true);
        }

        t2rUpdateMainWindowSize(t2rPositionChildWindows());

        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        GetScrollInfo(hwndMain, SB_VERT, &si);
        // Set the scroll position to the bottom
        si.nPos = si.nMax - si.nPage;
        SetScrollInfo(hwndMain, SB_VERT, &si, true);

        t2rPositionChildWindows();

        InvalidateRect(hwndMain, NULL, true);
        UpdateWindow(hwndMain);
    }

    return retVal;
}

// Clear display of export file name(s).
// Called whenever any parameter change would result in a change of export file content.
void t2rClearExportedFileDisplay() {
    if (hwndExpFileLbl) {
        SetWindowTextW(hwndExpFileLbl, L"");
        SetWindowTextW(hwndExpFile, L"");
        ShowWindow(hwndExpFileLbl, SW_HIDE);
        ShowWindow(hwndExpFile, SW_HIDE);
    }
}
