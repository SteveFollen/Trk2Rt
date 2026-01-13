// Track to Route Converter - Trk2Rt.h
// by Steve Follen, October 2023 - December 2025

#pragma once

#include "resource.h"
#include "framework.h"
#include <windowsx.h>
#include <shobjidl.h> // for file open dialog
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>   // for output formatiing setw()
#include <shlwapi.h> // for PathFileExistsW()
#include <fileapi.h> // for CreateDirectoryW()
#include <windef.h>  // for SIZE
#include "rapidxml_utils.hpp" // the rapid xml parsing [header only] library
#include <filesystem>
#include "Trk2Rt_RetCode.h"

namespace fs = std::filesystem;


// Maximum lengths, including the null terminator (number of wide charaters, not bytes)
constexpr int MAX_IN_FILE_NAME_LEN = MAX_PATH - 13; // leave space to append "T2R\"_T2R_P01/n" onto the import file name for export file name(s)
constexpr int MAX_NAME_LEN = 80;
constexpr int MAX_BASE_NAME_LEN = 65; // must be enough smaller than MAX_NAME_LEN to leave space to append track point number
constexpr int MAX_PREFIX_LEN = 11;    // point name prefix
constexpr int MAX_START_NUM_LEN = 5;  // point numbering starting number, as text
constexpr int MAX_NAME_LEAD = 26;     // Complete lead for point name (prefix + number.subnumber + ": " and a null).
                                      // Assumes max of 999 via points and 9999 shaping points following any via point

// Earth Radius (feet) - approximate average 20,902,231 (3,959 miles)
constexpr int EARTH_RAD_FT = 20902231;

using namespace std;
using namespace rapidxml;

// Primary route point source: no pirmary route points, imported route, imported waypoints, or imported placemark points.
typedef enum prmRtPtSrc { PRPS_NO_PRP, PRPS_ROUTE, PRPS_WAYPOINT, PRPS_PMP } prmRtPtSrc_t;

// Track source: no track, imported route route point extensions, imported track, or imported linestring
typedef enum trackSrc { TS_NO_TRK, TS_ROUTE, TS_TRACK, TS_LINESTRING } trackSrc_t;

// Primary route point's position within a route:
// the first, a mid, or the last point of the route.
typedef enum rtPos { RTPOS_FIRST, RTPOS_MID, RTPOS_LAST } rtPos_t;

// Point Numbering Styles: All points continuous, via points continuous, none
typedef enum ptNumStyle { NS_ALL, NS_VIA, NS_NONE } ptNumStyle_t;

// Any point
class Point
{
public:
    double lat;  // latitude
    double lon;  // longitude
    Point() { lat = 0.0; lon = 0.0; };
};

// A point in a track
class TrackPoint : public Point
{
private:
    int idx; // index (track order)
    bool rdIdChg; // true if the track point indicates a road change
    TrackPoint* next; // next point in the track

    friend class Track;

public:
    TrackPoint(int i, double lt, double ln, bool change = false) { idx = i; lat = lt; lon = ln; rdIdChg = change; next = NULL; }

    void setRoadChange(void) {
        rdIdChg = true;
    }

    int getIdx() const {
        return idx;
    }

    bool isRdIdChg() const {
        return rdIdChg;
    }

};

// A track, made up of TrackPoints
class Track
{
private:
    TrackPoint* head;
    TrackPoint* tail;
    int ntrkPts;  // number of points in the track
    int nRdIdChg; // number of points in the track that involve a road id change; a subset of all trackpoints (subset of ntrkPts);

public:

    Track(void) { head = NULL; tail = NULL; ntrkPts = 0;  nRdIdChg = 0; }

    // add a point to the end of the track
    TrackPoint* addTrackPoint(double lt, double ln, bool change = false)
    {
        TrackPoint* pt = new TrackPoint(ntrkPts, lt, ln, change);
        if (head == NULL) {
            tail = head = pt;
        }
        else {
            tail->next = pt;
            tail = pt;
        }
        ntrkPts++;
        if (change) {
            nRdIdChg++;
        }
        return pt;
    }

    TrackPoint* getFirst(void)
    {
        return (TrackPoint*)head;
    }

    TrackPoint* getNext(TrackPoint* tp) {
        return tp->next;
    }

    TrackPoint* getLast(void)
    {
        return (TrackPoint*)tail;
    }

    // empty the track
    void empty(void)
    {
        TrackPoint* pt;
        while (head) {
            pt = (TrackPoint*)head;
            head = head->next;
            delete pt;
        }
        tail = NULL;
        ntrkPts = 0;
        nRdIdChg = 0;
    }

    int nPts() const
    {
        return ntrkPts;
    }

    int nRdChg() const
    {
        return nRdIdChg;
    }

};

class RoutePointList;  // forward declaration

// Primary route point
class RoutePoint : public Point
{
private:
    friend class RoutePointList;
    RoutePoint* next; // pointer to next point in the list; NULL if this is the tail of the list

public:
    wchar_t ptName[MAX_NAME_LEN]; // Original imported waypoint or route point name (text)
    bool   shPt;         // true for shaping point, false for via point
    TrackPoint* trkSeg;  // the first point of the segment of track between this route point and the next
    double clsTrkPtDist; // distance    to the closest track point (from this route point)
    int    clsTrkPtIdx;  // index       of the closest track point
    Point  clsPt;        // coordinates of the closet track point
    int    nTrkPts;      // number of intermediate track points between this route point and the next
    int    nRdIdChg;     // number of intermediate track points between this route point and the next that incldue a road id change (subset of nTrkPts)
    int    nShpPts;      // number of additional shaping points between this route point and the next
    int    nShPtIdx;     // index into nShPtVal / hwndRtPtNSP for % of additional shaping points to export following this route point
    double shPtPC;       // % of additional shaping points to export following this route point
    rtPos_t posInRt;     // position in route - indicates start, mid, or end of a route
    bool   genEndPt;     // true for generated route endpoints

    // route point detail windows:
    HWND   hwndPtPrfx;    // point numbering prefix - edit box
    HWND   hwndPtPrfxLbl; // static label
    HWND   hwndPtStNum;   // point numbering starting number (as text) - edit
    HWND   hwndPtStNumLbl;// static label
    HWND   hwndRtPtLd;   // lead, the prefix and number - static window
    HWND   hwndRtPtName; // name - edit box
    HWND   hwndRtPtSP;   // export as a shaping point - checkbox
    HWND   hwndRtPtEx;   // exclude from route - checkbox
    HWND   hwndRtPtSlt;  // select to split route at this route point - checkbox
    HWND   hwndRtPtDist; // track point distance (clsTrkPtDist) - static window
    HWND   hwndRtPtNSP;  // percent of following shaping points - combobox
    HWND   hwndRtPtAdd;  // Additional shaping / track point info - static window

    RoutePoint(double lt, double lg, wchar_t* pNm) {
        lat = lt;  lon = lg; wcscpy_s(ptName, MAX_NAME_LEN, pNm); shPt = false; trkSeg = NULL;
        clsTrkPtDist = EARTH_RAD_FT;  clsTrkPtIdx = 0; clsPt.lat = 0.0; clsPt.lon = 0.0;
        nTrkPts = 0; nRdIdChg = 0; nShpPts = 0; nShPtIdx = 0; shPtPC = 0.0; posInRt = RTPOS_MID; genEndPt = false;
        hwndPtPrfx = NULL; hwndPtPrfxLbl = NULL; hwndPtStNum = NULL; hwndPtStNumLbl = NULL;
        hwndRtPtLd = NULL; hwndRtPtName = NULL;
        hwndRtPtSP = NULL; hwndRtPtEx = NULL; hwndRtPtSlt = NULL;
        hwndRtPtDist = NULL; hwndRtPtNSP = NULL; hwndRtPtAdd = NULL; next = NULL;
    };

    // for use by RoutePointList::split(RoutePoint* pt) to duplicate a route point
    RoutePoint(RoutePoint* pt) {
        lat = pt->lat;  lon = pt->lon; wcscpy_s(ptName, MAX_NAME_LEN, pt->ptName); shPt = false; trkSeg = pt->trkSeg;
        clsTrkPtDist = pt->clsTrkPtDist; clsTrkPtIdx = pt->clsTrkPtIdx; clsPt.lat = pt->clsPt.lat; clsPt.lon = pt->clsPt.lon;
        nTrkPts = 0; nRdIdChg = 0; nShpPts = 0; nShPtIdx = 0; shPtPC = 0.0; posInRt = RTPOS_MID; genEndPt = false;
        hwndPtPrfx = NULL; hwndPtPrfxLbl = NULL; hwndPtStNum = NULL; hwndPtStNumLbl = NULL;
        hwndRtPtLd = NULL; hwndRtPtName = NULL; hwndRtPtLd = NULL;
        hwndRtPtSP = NULL; hwndRtPtEx = NULL; hwndRtPtSlt = NULL;
        hwndRtPtDist = NULL; hwndRtPtNSP = NULL; hwndRtPtAdd = NULL; next = NULL;
    };

    ~RoutePoint()
    {
        if (hwndPtPrfx) {
            DestroyWindow(hwndPtPrfx);
        }
        if (hwndPtPrfxLbl) {
            DestroyWindow(hwndPtPrfxLbl);
        }
        if (hwndPtStNum) {
            DestroyWindow(hwndPtStNum);
        }
        if (hwndPtStNumLbl) {
            DestroyWindow(hwndPtStNumLbl);
        }
        if (hwndRtPtName) {
            DestroyWindow(hwndRtPtName);
        }
        if (hwndRtPtLd) {
            DestroyWindow(hwndRtPtLd);
        }
        if (hwndRtPtSP) {
            DestroyWindow(hwndRtPtSP);
        }
        if (hwndRtPtEx) {
            DestroyWindow(hwndRtPtEx);
        }
        if (hwndRtPtSlt) {
            DestroyWindow(hwndRtPtSlt);
        }
        if (hwndRtPtDist) {
            DestroyWindow(hwndRtPtDist);
        }
        if (hwndRtPtNSP) {
            DestroyWindow(hwndRtPtNSP);
        }
        if (hwndRtPtAdd) {
            DestroyWindow(hwndRtPtAdd);
        }
    }
};

// Primary route point and exclude route point lists
class RoutePointList
{
private:
    RoutePoint* head;
    RoutePoint* tail;
    int nRtPts;  // Count of total points in the list
    int nRoutes; // Count of total routes in the list
public:
    wchar_t routeName[MAX_NAME_LEN];

    RoutePointList(int nRts) { head = NULL; tail = NULL; nRtPts = 0; nRoutes = nRts;  memset(routeName, 0, sizeof(routeName)); }

    // Append a new point to the end of the list.
    // Return that new point.
    RoutePoint* appendLast(double lt, double lg, wchar_t* pNm)
    {
        RoutePoint* newPoint = new RoutePoint(lt, lg, pNm);
        if (head == NULL) {
            head = newPoint;
            newPoint->posInRt = RTPOS_LAST; // in appendLast() here - can't be both first and last - need at least 2 points for a route anyway
            tail = head;
        }
        else {
            if (tail == head) {
                head->posInRt = RTPOS_FIRST;
            }
            else {
                tail->posInRt = RTPOS_MID;
            }
            tail->next = newPoint;
            tail = newPoint;
            tail->posInRt = RTPOS_LAST;
        }
        nRtPts++;
        return newPoint;
    }

    // Create a new route point and insert it at the head of the list
    RoutePoint* createFirst(double lt, double lg, wchar_t* pNm)
    {
        RoutePoint* newPoint = new RoutePoint(lt, lg, pNm);
        if (head == NULL) {
            head = newPoint;
            head->posInRt = RTPOS_FIRST; // can't be both first and last - need at least 2 points for a route anyway
            tail = head;
        }
        else {
            newPoint->next = head;
            newPoint->next->posInRt = RTPOS_MID;
            head = newPoint;
            head->posInRt = RTPOS_FIRST;
            tail->posInRt = RTPOS_LAST;  // in case there was only one point (the head) in the list
        }
        nRtPts++;
        return newPoint;
    }

    // Insert pt into the list in closest track point index order.
    // pt must be valid.
    // Assumes list is already in order - should only be called after reorderPerTrack()
    // This is called only by t2rUpdateRtPtLists() which is called only:
    // - when a route point is being moved to excPtList, where posInRt irrelevent, and
    // - when a route point is being moved from excPtList back to prmRtPtList.
    // Since changing exclusion is blocked at the first route split, only the head and tail
    // of the prmRtPtList can have posInRt of RTPOS_FIRST or RTPOS_LAST.
    // Returns the point previous to the one just inserted, that is the point preceeding pt in the ordered list
    RoutePoint* orderedInsert(RoutePoint* pt)
    {
        RoutePoint* current = head;
        RoutePoint* next = NULL;
        RoutePoint* prev = NULL;
        if (!current) {
            // list is empty; pt becomes the first and only point in the list
            // pt->next should be NULL already
            head = tail = pt;
            head->posInRt = RTPOS_FIRST; // can't be both first and last - need at least 2 points for a route anyway
        }
        else {
            // there is at least one point in the list
            next = head->next;
            while (current) {
                if (current->clsTrkPtIdx > pt->clsTrkPtIdx) {
                    // insert pt before current
                    if (current == head) {
                        pt->next = head;
                        pt->posInRt = RTPOS_FIRST;
                        if (head == tail) {
                            pt->next->posInRt = RTPOS_LAST;
                        }
                        else {
                            pt->next->posInRt = RTPOS_MID;
                        }
                        head = pt;
                    }
                    else {
                        // cuurent is after head, so can't be RTPOS_FIRST, and prev must be vaild.
                        // pt belongs between prev and current
                        prev->next = pt;
                        pt->posInRt = RTPOS_MID;
                        pt->next = current;
                    }
                    break;
                }
                else if (!current->next) {
                    // pt belongs at the end of list
                    // current == tail
                    tail->posInRt = RTPOS_MID;
                    pt->posInRt = RTPOS_LAST;
                    tail = current->next = pt;
                    // pt->next should already be NULL
                    break;
                }
                else {
                    // keep looking
                    prev = current;
                    current = current->next;
                }
            }
        }
        nRtPts++;
        return prev;
    }

    // Remove a point from the list, but do not delete it.
    // Returns the removed point, or NULL if pt was not in the list.
    // This is called only by t2rUpdateRtPtLists() which is called only:
    // - when a route point is being moved between prmRtPtList and excPtList, in either direction.
    // posInRt is irrelevent in excPtList and handled by orderedInsert for prmRtPtList.
    RoutePoint* remove(RoutePoint* pt)
    {
        RoutePoint* current = head;
        RoutePoint* prev = NULL;
        while (current) {
            if (current == pt) {
                if ((current == head) && (current == tail)) {
                    // current is the only point in the list
                    head = tail = NULL;
                }
                else if (current == head) {
                    // there are at least two points in the list
                    head = head->next;
                }
                else if (current == tail) {
                    // there are at least two points in the list
                    // pt is not the head
                    // so prev must be valid
                    prev->next = NULL;
                    tail = prev;
                }
                else {
                    // there are more than two points in the list.
                    // pt is neither head nor tail.
                    // both prev and current->next must be vaild.
                    prev->next = current->next;
                }
                current->next = NULL;
                break;
            }
            prev = current;
            current = current->next;
        }
        if (current) {
            nRtPts--;
        }
        return current;
    }

    // Disable the exclude checkbox for all points in the list.
    // This is intended to be a one-time non-reversible function to be applied when
    // the route is first split.  Excluding / including route points from / to split routes
    // would require code complexity that is not worth the functionality it would provide.
    void disableExclude(void)
    {
        RoutePoint* pt = this->getFirst();
        while (pt) {
            if (pt->hwndRtPtEx) {
                EnableWindow(pt->hwndRtPtEx, false);
            }
            pt = pt->next;
        }
    }

    // Duplicate a route point for route splitting.
    // Insert the new point into prmRtPtList in front of the point being duplicated.
    // The new point becomes the end of the current route and
    // the duplicated point (pt) becomes the beginning of the next route.
    // Note that the first and last points in a route can never be split.
    RoutePoint* split(RoutePoint* pt)
    {
        RoutePoint* newPoint = new RoutePoint(pt);

        newPoint->posInRt = RTPOS_LAST;
        pt->posInRt = RTPOS_FIRST;
        newPoint->next = pt;
        RoutePoint* wlkPt = this->getFirst();
        while (wlkPt) {
            if (wlkPt->next == pt) {
                wlkPt->next = newPoint;
                break;
            }
            wlkPt = wlkPt->next;
        }
        nRtPts++;
        nRoutes++;

        return newPoint;
    }

    RoutePoint* getFirst()
    {
        return head;  // null if the list is empty
    }

    RoutePoint* getNext(RoutePoint* pt)
    {
        return pt->next;  // null if pt is the last in the list
    }

    RoutePoint* getLast()
    {
        return tail;  // null if the list is empty
    }

    int nPts() const
    {
        return nRtPts;
    }

    int getNumRoutes() const
    {
        return nRoutes;
    }

    // Clear all trackpoint data from all route points
    void clearTrackData(void)
    {
        RoutePoint* pt = head;
        while (pt) {
            pt->clsTrkPtDist = EARTH_RAD_FT;
            pt->clsTrkPtIdx = 0;
            pt->trkSeg = NULL;
            pt->clsPt.lat = 0.0;
            pt->clsPt.lon = 0.0;
            pt = pt->next;
        }
    }

    // Reorder the list of route points to be in track point order.
    // That is, sort in order of increasing closest track point index.
    // Return true if any change in order occurred.
    // Return false if the route point list was already in track point order.
    // This is intended to be called only immediately after an import file is parsed, before any route split occurs
    bool reorderPerTrack() {
        RoutePoint* current;
        RoutePoint* prev;
        RoutePoint* currentNext;
        bool reordered = false;
        bool swapped;
        int count = 0;

        // Iterating over the whole linked list
        while (count < nRtPts) {
            current = head;
            prev = head;
            swapped = false;

            while (current->next) {
                currentNext = current->next;
                if (current->clsTrkPtIdx > currentNext->clsTrkPtIdx) {
                    swapped = true;
                    reordered = true;
                    if (current == head) {
                        current->next = currentNext->next;
                        currentNext->next = current;
                        prev = currentNext;
                        head = prev;
                    }
                    else {
                        current->next = currentNext->next;
                        currentNext->next = current;
                        prev->next = currentNext;
                        prev = currentNext;
                    }
                    continue;
                }
                prev = current;
                current = current->next;
            }
            if (!swapped) {
                break; // the inner while loop
            }
            ++count;
        }
        if (reordered) {
            // need to fix both posInRt and tail
            current = head;
            head->posInRt = RTPOS_FIRST;
            while (current->next) {
                current = current->next;
                current->posInRt = RTPOS_MID;
            }
            tail = current;
            tail->posInRt = RTPOS_LAST;
        }
        return reordered;
    }

    // Empty the list, deleting all points
    void empty() {
        RoutePoint* pt;
        while (head) {
            pt = head;
            head = head->next;
            delete pt;
        }
        head = NULL;
        tail = NULL;
        nRtPts = 0;
        nRoutes = 1;
        memset(routeName, 0, sizeof(routeName));
    }
};

// Global Variables:
extern WCHAR ptNameBase[MAX_BASE_NAME_LEN]; // Base for shaping point naming; track point number will be appended to this base
extern WCHAR importPathname[MAX_PATH]; // Complete path and file name of the import file
extern WCHAR exportPathname[MAX_PATH]; // Complete path and file name of the export file
extern int nShPtIdx; // Index for global % of intermediate track points to export as additional shaping points between each primary route point pair (nShPtVal / hwndNumShPt).
                     // Note that this initially seeds and, on change, overwrites the route point specific nShPtIdx / hwndRtPtNSP.
extern Track theTrack; // the track. Only one track per import is supported.
extern RoutePointList prmRtPtList; // Linked list of primary route points which are included in the route(s).
extern RoutePointList excPtList;   // Linked list of primary route points which are excluded from the route(s).
extern prmRtPtSrc_t prmRtPtSource; // primary route point source, none route, or waypoints
extern trackSrc_t trackSource;     // track source - none, route, or track
extern bool runBkGrnd; //true to run windowless in the background - supports commnad line execution
extern bool parseImFl; // true if there is an import file from the command line to be parsed
extern bool expWpt;  // export waypoints
extern bool expTrk;  // export track
extern bool expRt;   // export route
extern bool reLocPrmRtPt; // relocate each primary route point to its closest track point
extern bool stripPrev; // strip previous prefix and numbering, if any, from imported route points
extern ptNumStyle_t numStyIdx; // point numbering style on export

extern int cxChar; // average character width (pixels)

extern wchar_t  nShPtTextTrk[6][6]; // Additional shaping point % options for track points and linestring coordinates
extern int      nShPtValTrk[6];// Additional shaping point % values for track points and linestring coordinates
extern wchar_t  nShPtTextRt[6][7];  // Additional shaping point % options for road changes
extern int      nShPtValRt[6]; // Additional shaping point % values for road changes
extern int* nShPtVal; // pointer to whichever of the above value arrays is appropriate
extern double shPtPC; // global % of additional shaping points to export following each route point

extern bool importedNum; // true if imported route included previous numbering - indicated by ": " in a route point name

extern HWND hwndMain;         // Parent window
extern HWND hwndOpeningLbl;   // Initial display at startup>hwndRtPtDist
extern HWND hwndImportInfo;   // Import information - static
extern HWND hwndUpSep;        // upper seperator
extern HWND hwndExpWpCheck;   // Export Waypoints - checkbox
extern HWND hwndExpTrkCheck;  // Export Track - checkbox
extern HWND hwndExpRtCheck;   // Export Route - checkbox
extern HWND hwndRouteName;    // Track / route name - edit
extern HWND hwndVpLocCheck;   // Route point re-location - checkbox
extern HWND hwndNumShPtLbl;   // Convert - label
extern HWND hwndNumShPtLbl2;  // % of intermediate ___ to additional shaping points between each primary route point pair - label
extern HWND hwndNumShPt;      // % of intermediate ___ to additional shaping points between each primary route point pair - combo box
extern HWND hwndPtName;       // Additional point name - edit
extern HWND hwndRtPtNameCheck;// Use route point naming  - checkbox
extern HWND hwndPtNumSty;     // Point numbering style - combo box
extern HWND hwndRemoveButton; // Remove previous numbering button
extern HWND hwndDetailLbl;    // Route point detail label
extern HWND hwndShapeLbl;     // Shaping label for route point options checkbox
extern HWND hwndExcludeLbl;   // Exclude label for route point options checkbox
extern HWND hwndDistlLbl;     // Distance label route point distance from closest track point
extern HWND hwndSplitlLbl;    // Split label for route point options checkbox
extern HWND hwndDistUnits;    // Distance units - combobox
extern HWND hwndSplitButton;  // Split route - button
extern HWND hwndExportButton; // Export - button
extern HWND hwndExportInfo;   // Export information - static
extern HWND hwndExpFileLbl;   // Exported File label "route exported to"
extern HWND hwndExpFile;      // Exported File Display
extern HWND hwndExcSep;       // Seperator above excluded route points
extern HWND hwndExcListLbl;   // Excluded route points list label
extern HWND hwnd2ndExcLbl;    // Second exclude label; this one is for the excluded route points list

// Forward declarations
// Import file and parse related
LRESULT t2rGetImportFileName();
tr2RC t2rParseImportFile();
// Export file related
LRESULT t2rGetExportFileName(bool dialog);
tr2RC t2rExport();
void t2rClearExportedFileDisplay();
// child window positioning and main (parent) window sizing
int  t2rPositionChildWindows();
void t2rUpdateMainWindowSize(int ySzClient);
// window creation
void t2rCreatePrmRtPtDetailWindows(void);
// update on global parameter change
void t2rUpdateGlobalTrkPtPercent(bool setSelection);
// assure valid route point ends
void t2rCheckAddViaPtsAtTrackEnds(void);
// show or hide the set of global configuration options
void showGlobalConfig(bool show);
// Determine the number of intermediate track points and road id changes, if any, between each pair of route points.
void t2rSetIntermediateCounts(void);
