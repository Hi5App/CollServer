#include "vn_app2.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#include "colldetection.h"
#include "coll_server.h"
#include "sort_swc.h"
#include <iostream>
#include <vector>
#include <QHttpMultiPart>
#include "detect_crossing/utilities.h"
#include "detect_crossing/SwcReader.h"
#include <filesystem>
#include "detect_crossing/CrossingDetect.h"
#include "detect_crossing/ResultWriter.h"
#include "service/RpcCall.h"
#include "service/WrappedCall.h"
#include "grpcpp/grpcpp.h"

XYZ CollDetection::maxRes = XYZ(0, 0, 0);
XYZ CollDetection::subMaxRes;

CollDetection::CollDetection(CollServer* curServer, string brainServerIP, string superuserServerIP, string neuronfiberSegServerIP, string brainServerPort, string superuserServerPort, string neuronfiberSegServerPort, QObject* parent):myServer(static_cast<CollServer*>(parent)){
    myServer=curServer;
    accessManager=new QNetworkAccessManager(this);
    timerForFilterTip=new QTimer(this);
//    SuperUserHostAddress="http://114.117.165.134:26000/SuperUser";
//    BrainTellHostAddress="http://114.117.165.134:26000/dynamic";
    SuperUserHostAddress="http://"+QString::fromStdString(superuserServerIP)+":"+QString::fromStdString(superuserServerPort)+"/SuperUser";
    BrainTellHostAddress="http://"+QString::fromStdString(brainServerIP)+":"+QString::fromStdString(brainServerPort)+"/release";
    neuronFiberSegUrl="http://"+QString::fromStdString(neuronfiberSegServerIP)+":"+QString::fromStdString(neuronfiberSegServerPort)+"/api/inference";
    tipDirPath="/home/BrainTellServer/tmpDirForPredict/tip";
    segDirPath="/home/BrainTellServer/tmpDirForPredict/neuron_fiber";
}

XYZ CollDetection::getSomaCoordinate(QString apoPath){
    myServer->isSomaExists=false;
    XYZ coordinate;
    coordinate.x=-1;
    coordinate.y=-1;
    coordinate.z=-1;
    QFile qf(apoPath);
    if (!qf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug()<<"apofile open error";
        return coordinate;
    }
    char *buf;
    char _buf[1000];
    qf.readLine(_buf, sizeof(_buf));
    for (buf=_buf; (*buf && *buf==' '); buf++);
    if (buf[0]=='#' ||buf[0]=='\0')
    {
        if(!qf.atEnd())
        {
            qf.readLine(_buf, sizeof(_buf));
            for (buf=_buf; (*buf && *buf==' '); buf++);
        }
        else{
            qDebug()<<"apofile format error";
            return coordinate;
        }
    }
    else{
        qDebug()<<"apofile format error";
        return coordinate;
    }
    QStringList qsl = QString(buf).split(",");
    if (qsl.size()==0){
        qDebug()<<"apofile format error";
        return coordinate;
    }
    else{
        for (int i=4; i<qsl.size(); i++)
        {
            qsl[i].truncate(200); //change from 99 to 200, 20121212, by PHC
            if (i==4) coordinate.z = qsl[i].toFloat();
            if (i==5) coordinate.x = qsl[i].toFloat();
            if (i==6)
            {
                coordinate.y = qsl[i].toFloat();
                myServer->isSomaExists=true;
                break;
            }
        }
    }
    return coordinate;
}

void CollDetection::detectWholeAtStart(){
    detectOthersWhole();
    detectLoops();
    detectTipsWhole();
//    detectBranchingPoints();
}

void CollDetection::detectTips(){
    qDebug()<<"enter detectTips";
    map<string, set<size_t>> allPoint2SegIdMap = getWholeGrid2SegIDMap(myServer->segments);
    V_NeuronSWC_list last3MinSegments = myServer->last3MinSegments;
    myServer->last3MinSegments.seg.clear();
    getSegmentsForMissingDetect(last3MinSegments, myServer->segmentsForMissingDetect, myServer->segments);

    tipPoints=tipDetection(myServer->segmentsForMissingDetect, false, allPoint2SegIdMap, 30);
//    QString apoFileNameTip="/home/BrainTellServer/image/tmpApoFile/tip/"+myServer->getAnoName()+"_tip.apo";
//    getApoForCrop(apoFileNameTip, tipPoints);
//    myServer->imediateSave();
    myServer->segmentsForMissingDetect.seg.clear();
    handleTip(tipPoints);
    tipPoints.clear();
//    tipPoints=tipDetection(myServer->segments, true, 20);
//    myServer->imediateSave();

//    tipPoints=tipDetection(myServer->segments, false, 20);
//    QString apoFileNameUndone="/home/BrainTellServer/image/tmpApoFile/undone/"+myServer->getAnoName()+"_undone.apo";
//    getApoForCrop(apoFileNameUndone, tipPoints);
//    myServer->imediateSave();
//    handleTip(tipPoints);
}

void CollDetection::detectTipsWhole(){
    map<string, set<size_t>> allPoint2SegIdMap = getWholeGrid2SegIDMap(myServer->segments);
    tipPoints=tipDetection(myServer->segments, false, allPoint2SegIdMap, 30);

    handleTip(tipPoints);
    tipPoints.clear();
    if(!myServer->getTimerForDetectTip()->isActive())
        myServer->startTimerForDetectTip();
}

void CollDetection::detectBranchingPoints(){
    int count=0;
    vector<NeuronSWC> outputSpecialPoints = branchingDetection(myServer->segments, 12);
    handleBranchingPoints(outputSpecialPoints, count);
}

void CollDetection::detectCrossings(){
    qDebug()<<"enter detectCrossings";
//    myServer->mutex.lock();
//    myServer->imediateSave(false);
//    myServer->mutex.unlock();
    QJsonArray infos = crossingDetection();
    handleCrossing(infos);
}

void CollDetection::detectOthers(){
    myServer->mutex.lock();
    myServer->mutexForDetectOthers.lock();
    myServer->mutexForDetectMissing.lock();
    getSegmentsForOthersDetect(myServer->last1MinSegments, myServer->segmentsForOthersDetect, myServer->segments);
    removeShortSegs(myServer->segmentsForOthersDetect);
    removeOverlapSegs(myServer->segmentsForOthersDetect);

    int count1=0;
    int count2=0;
    vector<NeuronSWC> outputSpecialPoints = specStructsDetection(myServer->segmentsForOthersDetect);
    myServer->last1MinSegments.seg.clear();
    myServer->segmentsForOthersDetect.seg.clear();
    myServer->mutexForDetectMissing.unlock();
    myServer->mutexForDetectOthers.unlock();
    myServer->mutex.unlock();

    vector<NeuronSWC> bifurPoints;
    vector<NeuronSWC> mulfurPoints;

    for(int i=0;i<outputSpecialPoints.size();i++){
        if(outputSpecialPoints[i].type == 6)
            bifurPoints.push_back(outputSpecialPoints[i]);
        else if(outputSpecialPoints[i].type == 8)
            mulfurPoints.push_back(outputSpecialPoints[i]);
    }

    handleMulFurcation(mulfurPoints, count1);
    handleNearBifurcation(bifurPoints, count2);

}

void CollDetection::detectOthersWhole(){
//    myServer->mutex.lock();
    int count1=0;
    int count2=0;
    myServer->mutex.lock();
    myServer->mutexForDetectOthers.lock();
    myServer->mutexForDetectMissing.lock();
    tuneErrorSegs(false);
    removeShortSegs(myServer->segments);
    removeOverlapSegs(myServer->segments);
    vector<NeuronSWC> outputSpecialPoints = specStructsDetection(myServer->segments);
    myServer->mutexForDetectMissing.unlock();
    myServer->mutexForDetectOthers.unlock();
    myServer->mutex.unlock();

    vector<NeuronSWC> bifurPoints;
    vector<NeuronSWC> mulfurPoints;

    for(int i=0;i<outputSpecialPoints.size();i++){
        if(outputSpecialPoints[i].type == 6)
            bifurPoints.push_back(outputSpecialPoints[i]);
        else if(outputSpecialPoints[i].type == 8)
            mulfurPoints.push_back(outputSpecialPoints[i]);
    }

    handleMulFurcation(mulfurPoints, count1);
    handleNearBifurcation(bifurPoints, count2);
}

void CollDetection::detectLoops(){
    int count=0;
    myServer->mutex.lock();
    myServer->mutexForDetectOthers.lock();
    myServer->mutexForDetectMissing.lock();
    tuneErrorSegs(false);
    vector<NeuronSWC> outputSpecialPoints = loopDetection(myServer->segments);
    myServer->mutexForDetectMissing.unlock();
    myServer->mutexForDetectOthers.unlock();
    myServer->mutex.unlock();

    handleLoop(outputSpecialPoints, count);
}

vector<NeuronSWC> CollDetection::specStructsDetection(V_NeuronSWC_list& inputSegList, double dist_thresh){
    vector<NeuronSWC> outputSpecialPoints;
    if(inputSegList.seg.size() == 0)
        return outputSpecialPoints;

//    bool flag = true;
//    if(inputSegList.seg.size() != myServer->segmentsForOthersDetect.seg.size())
//        flag = false;

//    if(flag){
//        removeOverlapSegs(inputSegList);
//    }

    map<string, set<size_t> > wholeGrid2segIDmap;
    map<string, bool> isEndPointMap;
    map<string, set<string>> parentMap;

    set<string> allPoints;
    map<string, set<string>> childMap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        vector<int> rowN2Index(seg.row.size()+1);

        for(size_t j=0; j<seg.row.size(); ++j){
            rowN2Index[seg.row[j].n]=j;
        }

        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            wholeGrid2segIDmap[gridKey].insert(size_t(i));
            allPoints.insert(gridKey);

            if(seg.row[j].parent!=-1){
                float x2Label=seg.row[rowN2Index[seg.row[j].parent]].x;
                float y2Label=seg.row[rowN2Index[seg.row[j].parent]].y;
                float z2Label=seg.row[rowN2Index[seg.row[j].parent]].z;
                QString parentKeyQ=QString::number(x2Label) + "_" + QString::number(y2Label) + "_" + QString::number(z2Label);
                string parentKey=parentKeyQ.toStdString();
                parentMap[gridKey].insert(parentKey);
                childMap[parentKey].insert(gridKey);
            }

            if(j == 0 || j == seg.row.size() - 1){
                isEndPointMap[gridKey] = true;
            }
        }
    }

    //    for(map<string, set<size_t> >::iterator it = wholeGrid2segIDmap.begin(); it != wholeGrid2segIDmap.end(); ++it){
    //        if(it->second.size() > 5){
    //            qDebug()<<it->first.c_str()<<" "<<it->second.size();
    //        }

    //    }


    //末端点和分叉点
    vector<string> points;
    vector<set<int>> linksIndex;
    //    vector<vector<int>> linksIndexVec;
    map<string,int> pointsIndexMap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(j==0 || j==seg.row.size()-1){
                //在pointsIndexMap中找不到某个线的末端点
                if(pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }else{
                if(wholeGrid2segIDmap[gridKey].size()>1 &&
                    isEndPointMap.find(gridKey) != isEndPointMap.end() &&
                    pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }
        }
    }
    qDebug()<<"points size: "<<points.size();

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        vector<int> segIndexs;
        set<int> segIndexsSet;
        segIndexs.clear();
        segIndexsSet.clear();
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(pointsIndexMap.find(gridKey) != pointsIndexMap.end()){
                int index = pointsIndexMap[gridKey];
                if(segIndexsSet.find(index) == segIndexsSet.end()){
                    segIndexs.push_back(index);
                    segIndexsSet.insert(index);
                }
            }
        }
        //        qDebug()<<"i : "<<i<<"seg size: "<<seg.row.size()<<" segIndexsSize: "<<segIndexs.size();
        for(size_t j=0; j<segIndexs.size()-1; ++j){
            if(segIndexs[j] == 1 || segIndexs[j+1] == 1){
                qDebug()<<segIndexs[j]<<" "<<segIndexs[j+1];
            }
            linksIndex[segIndexs[j]].insert(segIndexs[j+1]);
            //            linksIndexVec[segIndexs[j]].push_back(segIndexs[j+1]);
            linksIndex[segIndexs[j+1]].insert(segIndexs[j]);
            //            linksIndexVec[segIndexs[j+1]].push_back(segIndexs[j]);
        }
    }

    qDebug()<<"link map end";

    for(size_t i=0; i<points.size(); ++i){
//        qDebug()<<i<<" link size: "<<linksIndex[i].size();
        if(linksIndex[i].size() > 3){
            qDebug()<<i<<" link size: "<<linksIndex[i].size();
            NeuronSWC s;
            stringToXYZ(points[i],s.x,s.y,s.z);
            s.type = 8;
            outputSpecialPoints.push_back(s);
        }
    }

    vector<vector<size_t>> pairs;
    set<size_t> pset;

    size_t pre_tip_id=-1;
    size_t cur_tip_id=-1;

    double soma_radius=30;
    for(size_t i=0; i<points.size(); i++){
        if(linksIndex[i].size() == 3){
            pre_tip_id=cur_tip_id;
            cur_tip_id=i;
            if(pre_tip_id!=-1){
                NeuronSWC n1;
                stringToXYZ(points[pre_tip_id],n1.x,n1.y,n1.z);
                n1.type=6;
                NeuronSWC n2;
                stringToXYZ(points[cur_tip_id],n2.x,n2.y,n2.z);
                n2.type=6;
                set<size_t> n1Segs=wholeGrid2segIDmap[points[pre_tip_id]];
                set<size_t> n2Segs=wholeGrid2segIDmap[points[cur_tip_id]];
                int count1=0,count2=0;
                for(auto it1=n1Segs.begin();it1!=n1Segs.end();it1++)
                {
//                    qDebug()<<*it1;
//                    qDebug()<<getSegLength(inputSegList.seg[*it1]);
                    if(getSegLength(inputSegList.seg[*it1])>40)
                        count1++;
                }

                for(auto it2=n2Segs.begin();it2!=n2Segs.end();it2++)
                {
//                    qDebug()<<*it2;
//                    qDebug()<<getSegLength(inputSegList.seg[*it2]);
                    if(getSegLength(inputSegList.seg[*it2])>40)
                        count2++;
                }
//                qDebug()<<"n2Segs end";
                if(!(count1>=2&&count2>=2)){
                    continue;
                }
                if(myServer->isSomaExists){
                    if(distance(n1.x,myServer->somaCoordinate.x,n1.y,myServer->somaCoordinate.y,n1.z,myServer->somaCoordinate.z)>soma_radius
                        &&distance(n2.x,myServer->somaCoordinate.x,n2.y,myServer->somaCoordinate.y,n2.z,myServer->somaCoordinate.z)>soma_radius){
                        double dist=distance(n1.x,n2.x,n1.y,n2.y,n1.z,n2.z);
                        if(distance((n1.x+n2.x)/2,myServer->somaCoordinate.x,(n1.y+n2.y)/2,myServer->somaCoordinate.y,(n1.z+n2.z)/2,myServer->somaCoordinate.z)>1e-7&&dist<dist_thresh){
                            vector<size_t> v={pre_tip_id,cur_tip_id};
                            pairs.push_back(v);
                            pset.insert(pre_tip_id);
                            pset.insert(cur_tip_id);
                        }
                    }
                }
                else{
                    double dist=distance(n1.x,n2.x,n1.y,n2.y,n1.z,n2.z);
                    if(dist<dist_thresh){
                        vector<size_t> v={pre_tip_id,cur_tip_id};
                        pairs.push_back(v);
                        pset.insert(pre_tip_id);
                        pset.insert(cur_tip_id);
                    }
                }

            }
        }
    }

    qDebug()<<pairs;
    //    qDebug()<<points;

    for(auto it=pset.begin(); it!=pset.end(); it++){
        qDebug()<<*it;
        NeuronSWC n;
        stringToXYZ(points[*it],n.x,n.y,n.z);
        n.type=6;
        outputSpecialPoints.push_back(n);
    }

    return outputSpecialPoints;

}

vector<NeuronSWC> CollDetection::loopDetection(V_NeuronSWC_list& inputSegList, double dist_thresh){
    vector<NeuronSWC> outputSpecialPoints;
    outputSpecialPoints.clear();

    map<string, set<size_t> > wholeGrid2segIDmap;
    map<string, bool> isEndPointMap;
    map<string, set<string>> parentMap;

    set<string> allPoints;
    map<string, set<string>> childMap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        vector<int> rowN2Index(seg.row.size()+1);

        for(size_t j=0; j<seg.row.size(); ++j){
            rowN2Index[seg.row[j].n]=j;
        }

        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            wholeGrid2segIDmap[gridKey].insert(size_t(i));
            allPoints.insert(gridKey);

            if(seg.row[j].parent!=-1){
                float x2Label=seg.row[rowN2Index[seg.row[j].parent]].x;
                float y2Label=seg.row[rowN2Index[seg.row[j].parent]].y;
                float z2Label=seg.row[rowN2Index[seg.row[j].parent]].z;
                QString parentKeyQ=QString::number(x2Label) + "_" + QString::number(y2Label) + "_" + QString::number(z2Label);
                string parentKey=parentKeyQ.toStdString();
                parentMap[gridKey].insert(parentKey);
                childMap[parentKey].insert(gridKey);
            }

            if(j == 0 || j == seg.row.size() - 1){
                isEndPointMap[gridKey] = true;
            }
        }
    }

    //末端点和分叉点
    vector<string> points;
    vector<set<int>> linksIndex;
    //    vector<vector<int>> linksIndexVec;
    map<string,int> pointsIndexMap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(j==0 || j==seg.row.size()-1){
                //在pointsIndexMap中找不到某个线的末端点
                if(pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }else{
                if(wholeGrid2segIDmap[gridKey].size()>1 &&
                    isEndPointMap.find(gridKey) != isEndPointMap.end() &&
                    pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }
        }
    }

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        vector<int> segIndexs;
        set<int> segIndexsSet;
        segIndexs.clear();
        segIndexsSet.clear();
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(pointsIndexMap.find(gridKey) != pointsIndexMap.end()){
                int index = pointsIndexMap[gridKey];
                if(segIndexsSet.find(index) == segIndexsSet.end()){
                    segIndexs.push_back(index);
                    segIndexsSet.insert(index);
                }
            }
        }
        //        qDebug()<<"i : "<<i<<"seg size: "<<seg.row.size()<<" segIndexsSize: "<<segIndexs.size();
        for(size_t j=0; j<segIndexs.size()-1; ++j){
            if(segIndexs[j] == 1 || segIndexs[j+1] == 1){
                qDebug()<<segIndexs[j]<<" "<<segIndexs[j+1];
            }
            linksIndex[segIndexs[j]].insert(segIndexs[j+1]);
            //            linksIndexVec[segIndexs[j]].push_back(segIndexs[j+1]);
            linksIndex[segIndexs[j+1]].insert(segIndexs[j]);
            //            linksIndexVec[segIndexs[j+1]].push_back(segIndexs[j]);
        }
    }

    bool isDeleteEnd = false;
    while(!isDeleteEnd){
        isDeleteEnd = true;
        for(int i=0; i<points.size(); ++i){
            if(linksIndex[i].size() == 1){
                int linkIndex = *(linksIndex[i].begin());
                linksIndex[i].clear();
                linksIndex[linkIndex].erase(std::find(linksIndex[linkIndex].begin(),linksIndex[linkIndex].end(),i));
                isDeleteEnd = false;
            }
        }
    }

    //检测3条及3条以上的边构成的环

    vector<string> newpoints;

    for(size_t i=0; i<points.size(); ++i){
        if(linksIndex[i].size()>=2)
            newpoints.push_back(points[i]);
    }

    set<string> specPoints;
    size_t start=0;
    for(size_t i=0; i<newpoints.size(); ++i){
        //        qDebug()<<QString::fromStdString(newpoints[i])<<" "<<parentMap[newpoints[i]].size();
        /*if(newLinksIndexVec[i].size()>=2&&counts[i]>=3&&newLinksIndexVec[i].size()!=counts[i])*/
        if(parentMap[newpoints[i]].size()>=2){
            //            size_t interval=i-start;
            //            //interval/12
            //            int nums=interval/8;
            //            for(int j=0;j<nums;j++){
            //                specPoints.insert(newpoints[start+(j+1)*8]);
            //            }

            specPoints.insert(newpoints[i]);
            //            start=i+1;
            //            qDebug()<<"loop exists";
        }

    }

    //    if(start<newpoints.size()){
    //        size_t interval=newpoints.size()-1-start;
    //        int nums=interval/8;
    //        for(int j=0;j<nums;j++){
    //            specPoints.insert(newpoints[start+(j+1)*8]);
    //        }
    //    }

    //检测2条边构成的环
    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        //        if(seg.row.size()<4)
        //            continue;
        float xLabel1 = seg.row[0].x;
        float yLabel1 = seg.row[0].y;
        float zLabel1 = seg.row[0].z;
        float xLabel2=seg.row[seg.row.size()-1].x;
        float yLabel2=seg.row[seg.row.size()-1].y;
        float zLabel2=seg.row[seg.row.size()-1].z;
        QString gridKeyQ1 = QString::number(xLabel1) + "_" + QString::number(yLabel1) + "_" + QString::number(zLabel1);
        string gridKey1 = gridKeyQ1.toStdString();
        QString gridKeyQ2 = QString::number(xLabel2) + "_" + QString::number(yLabel2) + "_" + QString::number(zLabel2);
        string gridKey2 = gridKeyQ2.toStdString();
        if(gridKey1 == gridKey2){
            continue;
        }
        set<size_t> segSet1=wholeGrid2segIDmap[gridKey1];
        set<size_t> segSet2=wholeGrid2segIDmap[gridKey2];
        set<size_t> intersectionSet;
        set_intersection(segSet1.begin(),segSet1.end(),segSet2.begin(),segSet2.end(),inserter( intersectionSet , intersectionSet.begin()));
        vector<size_t> maybeOverlapSegIdVec;
        set<size_t> overlapSegIdSet;

        if(intersectionSet.size()>=2){
            for(auto it=intersectionSet.begin(); it!=intersectionSet.end(); it++){
                maybeOverlapSegIdVec.push_back(*it);
            }
            for(int m=0; m<maybeOverlapSegIdVec.size(); m++){
                for(int n=m+1; n<maybeOverlapSegIdVec.size(); n++){
                    int result = isOverlapOfTwoSegs(inputSegList.seg[maybeOverlapSegIdVec[m]], inputSegList.seg[maybeOverlapSegIdVec[n]]);
                    if(result == 1)
                        overlapSegIdSet.insert(maybeOverlapSegIdVec[m]);
                    if(result == 2)
                        overlapSegIdSet.insert(maybeOverlapSegIdVec[n]);
                }
            }

            if(maybeOverlapSegIdVec.size() - overlapSegIdSet.size() != 1){
                qDebug()<<"exists two-edge loop";
                qDebug()<<"gridKey1:"<<QString::fromStdString(gridKey1);
                qDebug()<<"gridKey2:"<<QString::fromStdString(gridKey2);
                specPoints.insert(gridKey1);
                specPoints.insert(gridKey2);
            }

            for(auto it=overlapSegIdSet.begin(); it!=overlapSegIdSet.end(); it++){
                //                qDebug()<<"to be deleted";
                //                qDebug()<<*it;
                inputSegList.seg[*it].to_be_deleted = true;
                auto seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), inputSegList.seg[*it]);
                if(seg_it != myServer->last1MinSegments.seg.end()){
                    seg_it->to_be_deleted = true;
                }

                seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), inputSegList.seg[*it]);
                if(seg_it != myServer->last3MinSegments.seg.end()){
                    seg_it->to_be_deleted = true;
                }
            }
        }
    }

    //检测线段相交导致的环
    for(auto it = wholeGrid2segIDmap.begin(); it != wholeGrid2segIDmap.end(); it++){
        if(it->second.size() == 2){
            bool isLoopExists = true;
            string coor = it->first;
            for(auto segIt = it->second.begin(); segIt != it->second.end(); segIt++){
                V_NeuronSWC seg = inputSegList.seg[*segIt];
                float xLabel1 = seg.row[0].x;
                float yLabel1 = seg.row[0].y;
                float zLabel1 = seg.row[0].z;
                float xLabel2=seg.row[seg.row.size()-1].x;
                float yLabel2=seg.row[seg.row.size()-1].y;
                float zLabel2=seg.row[seg.row.size()-1].z;
                QString gridKeyQ1 = QString::number(xLabel1) + "_" + QString::number(yLabel1) + "_" + QString::number(zLabel1);
                string gridKey1 = gridKeyQ1.toStdString();
                QString gridKeyQ2 = QString::number(xLabel2) + "_" + QString::number(yLabel2) + "_" + QString::number(zLabel2);
                string gridKey2 = gridKeyQ2.toStdString();
                if(coor == gridKey1 || coor == gridKey2){
                    isLoopExists = false;
                }
            }
            if(isLoopExists){
                specPoints.insert(coor);
            }
        }
    }

    auto iter = myServer->last1MinSegments.seg.begin();
    while (iter != myServer->last1MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last1MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->last3MinSegments.seg.begin();
    while (iter != myServer->last3MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last3MinSegments.seg.erase(iter);
        }
        else
            ++iter;


    QStringList result;
    int count = 0;
    proto::SwcDataV1 swcData;
    iter = inputSegList.seg.begin();
    while (iter != inputSegList.seg.end())
        if (iter->to_be_deleted){
            myServer->removedOverlapSegNum++;
            count++;
            result+=V_NeuronSWCToSendMSG(*iter);
            result.push_back("$");

            for(int j=0; j<iter->row.size(); j++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_x(iter->row[j].x);
                swcNodeInternalData.set_y(iter->row[j].y);
                swcNodeInternalData.set_z(iter->row[j].z);
                swcNodeInternalData.set_radius(iter->row[j].r);
                swcNodeInternalData.set_type(iter->row[j].type);
                swcNodeInternalData.set_mode(iter->row[j].creatmode);

                auto* newData = swcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
                newData->mutable_base()->set_uuid(iter->row[j].uuid);
            }
            iter = inputSegList.seg.erase(iter);
        }
        else
            ++iter;

    result.insert(0,QString("%1 server overlap %2 %3 %4").arg(0).arg(count).arg(123).arg(1));
    if(count!=0){
        proto::DeleteSwcNodeDataResponse response;
        WrappedCall::deleteSwcNodeData(myServer->swcUuid, swcData, response, myServer->cachedUserData);
        QString msg=QString("/delline_norm:"+result.join(","));
        qDebug()<<"removeOverLapSegs: "<<msg;
        emit myServer->clientSendMsgs({msg});
    }

    for(auto it=specPoints.begin(); it!=specPoints.end(); it++){
        NeuronSWC s;
        stringToXYZ(*it,s.x,s.y,s.z);
        s.type=0;
//        if(!myServer->isSomaExists)
        outputSpecialPoints.push_back(s);
//        else if(myServer->isSomaExists && distance(s.x,myServer->somaCoordinate.x,s.y,myServer->somaCoordinate.y,s.z,myServer->somaCoordinate.z)>dist_thresh)
//            outputSpecialPoints.push_back(s);
    }

    return outputSpecialPoints;
}

vector<NeuronSWC> CollDetection::tipDetection(V_NeuronSWC_list inputSegList, bool removeFlag, map<string, set<size_t>> allPoint2SegIdMap, double dist_thresh){
    vector<NeuronSWC> outputSpecialPoints;
    if(inputSegList.seg.size()==0)
        return outputSpecialPoints;

    if(maxRes.x==0 && maxRes.y==0 && maxRes.z==0)
        getImageMaxRES();
    map<string, bool> isEndPointMap;
    map<string, set<size_t> > wholeGrid2segIDmap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];

        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            wholeGrid2segIDmap[gridKey].insert(size_t(i));

            if(j == 0 || j == seg.row.size() - 1){
                isEndPointMap[gridKey] = true;
            }
        }
    }

    // 每条线的两个末端点
    vector<string> points;
    vector<set<int>> linksIndex;
    //    vector<vector<int>> linksIndexVec;
    map<string,int> pointsIndexMap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(j==0 || j==seg.row.size()-1){
                //在pointsIndexMap中找不到某个线的末端点
                if(pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }else{
                if(wholeGrid2segIDmap[gridKey].size()>1 &&
                    isEndPointMap.find(gridKey) != isEndPointMap.end() &&
                    pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }
        }
    }

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        vector<int> segIndexs;
        set<int> segIndexsSet;
        segIndexs.clear();
        segIndexsSet.clear();
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(pointsIndexMap.find(gridKey) != pointsIndexMap.end()){
                int index = pointsIndexMap[gridKey];
                if(segIndexsSet.find(index) == segIndexsSet.end()){
                    segIndexs.push_back(index);
                    segIndexsSet.insert(index);
                }
            }
        }
        //        qDebug()<<"i : "<<i<<"seg size: "<<seg.row.size()<<" segIndexsSize: "<<segIndexs.size();
        for(size_t j=0; j<segIndexs.size()-1; ++j){
            if(segIndexs[j] == 1 || segIndexs[j+1] == 1){
                qDebug()<<segIndexs[j]<<" "<<segIndexs[j+1];
            }
            linksIndex[segIndexs[j]].insert(segIndexs[j+1]);
            //            linksIndexVec[segIndexs[j]].push_back(segIndexs[j+1]);
            linksIndex[segIndexs[j+1]].insert(segIndexs[j]);
            //            linksIndexVec[segIndexs[j+1]].push_back(segIndexs[j]);
        }
    }

    qDebug()<<"link map end";

    //detect tips
    set<string> tips;

    for(int i=0;i<inputSegList.seg.size();i++){
        V_NeuronSWC seg = inputSegList.seg[i];
        float xLabel1 = seg.row[0].x;
        float yLabel1 = seg.row[0].y;
        float zLabel1 = seg.row[0].z;
        QString gridKeyQ1 = QString::number(xLabel1) + "_" + QString::number(yLabel1) + "_" + QString::number(zLabel1);
        string gridKey1 = gridKeyQ1.toStdString();
        float xLabel2 = seg.row[seg.row.size()-1].x;
        float yLabel2 = seg.row[seg.row.size()-1].y;
        float zLabel2 = seg.row[seg.row.size()-1].z;
        QString gridKeyQ2 = QString::number(xLabel2) + "_" + QString::number(yLabel2) + "_" + QString::number(zLabel2);
        string gridKey2 = gridKeyQ2.toStdString();
        if(wholeGrid2segIDmap[gridKey1].size()==1 && allPoint2SegIdMap[gridKey1].size()==1 && wholeGrid2segIDmap[gridKey2].size()>1)
        {
            if(myServer->isSomaExists&&sqrt((xLabel1-myServer->somaCoordinate.x)*(xLabel1-myServer->somaCoordinate.x)+
                (yLabel1-myServer->somaCoordinate.y)*(yLabel1-myServer->somaCoordinate.y)+(zLabel1-myServer->somaCoordinate.z)*(zLabel1-myServer->somaCoordinate.z))>50)
                tips.insert(gridKey1);
            else if(!myServer->isSomaExists)
                tips.insert(gridKey1);
        }
        if(wholeGrid2segIDmap[gridKey2].size()==1 && allPoint2SegIdMap[gridKey2].size()==1 && wholeGrid2segIDmap[gridKey1].size()>1)
        {
            if(myServer->isSomaExists&&sqrt((xLabel2-myServer->somaCoordinate.x)*(xLabel2-myServer->somaCoordinate.x)+
                (yLabel2-myServer->somaCoordinate.y)*(yLabel2-myServer->somaCoordinate.y)+(zLabel2-myServer->somaCoordinate.z)*(zLabel2-myServer->somaCoordinate.z))>50)
                tips.insert(gridKey2);
            else if(!myServer->isSomaExists)
                tips.insert(gridKey2);
        }
    }

    for(auto it=tips.begin();it!=tips.end();it++){
        vector<size_t> visitedSegIds;
        size_t segId=*wholeGrid2segIDmap[*it].begin();
        visitedSegIds.push_back(segId);
        V_NeuronSWC seg = inputSegList.seg[segId];
        float xLabel0 = seg.row[0].x;
        float yLabel0 = seg.row[0].y;
        float zLabel0 = seg.row[0].z;
        QString gridKeyQ0 = QString::number(xLabel0) + "_" + QString::number(yLabel0) + "_" + QString::number(zLabel0);
        string gridKey0 = gridKeyQ0.toStdString();

        float tipBranchLength = 0;
        bool isReverse = false;
        XYZ tipCoor = XYZ(xLabel0, yLabel0, zLabel0);
        if(wholeGrid2segIDmap[gridKey0].size() != 1)
        {
            isReverse = true;
            tipCoor = XYZ(seg.row[seg.row.size() - 1].x, seg.row[seg.row.size() - 1].y, seg.row[seg.row.size() - 1].z);
        }
        bool flag = true;

        MissingForSegData segData;
        segData.centerCoor = XYZ(16.0, 16.0, 16.0);
        segData.edgeCoor = XYZ(16.0, 16.0, 16.0);
        segData.type = seg.row[0].type;
        segData.maxResCoor = tipCoor;
        double maxEdge2CenterDist = 0;

        while(true){
            int size = seg.row.size();
            vector<int> indexs(size);
            for(int m = 0; m < size;  m++)
                indexs[m] = m;
            if(isReverse)
                reverse(indexs.begin(), indexs.end());
            for(int i = 0; i< size; i++){
                int curIndex = indexs[i];
                int nextIndex;
                if (i != size - 1){
                    nextIndex = indexs[i + 1];
                }

                float xLabel = seg.row[curIndex].x;
                float yLabel = seg.row[curIndex].y;
                float zLabel = seg.row[curIndex].z;
                QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                string gridKey = gridKeyQ.toStdString();
                vector<string>::iterator it2 = find(points.begin(), points.end(), gridKey);
                // 当前点是线的末端点
                if(it2 != points.end()){
                    int index2 = it2 - points.begin();
                    if(linksIndex[index2].size() >= 3){
                        flag = false;
                        break;
                    }
                    else{
                        if(i == size - 1)
                            break;
                    }
                }

                tipBranchLength += distance(xLabel, seg.row[nextIndex].x,
                                            yLabel, seg.row[nextIndex].y,
                                            zLabel, seg.row[nextIndex].z);
                double curDist = distance(seg.row[nextIndex].x, int(tipCoor.x), seg.row[nextIndex].y, int(tipCoor.y), seg.row[nextIndex].z, int(tipCoor.z));
                XYZ curRelCoor = XYZ((seg.row[nextIndex].x - int(tipCoor.x)) / 2, (seg.row[nextIndex].y - int(tipCoor.y)) / 2, (seg.row[nextIndex].z - int(tipCoor.z)) / 2);
                if(curDist > maxEdge2CenterDist && curRelCoor.x >= -16 && curRelCoor.x <= 15
                    && curRelCoor.y >= -16 && curRelCoor.y <= 15 && curRelCoor.z >= -16 && curRelCoor.z <= 15){

                    maxEdge2CenterDist = curDist;
                    segData.edgeCoor = XYZ(curRelCoor.x + 16, curRelCoor.y + 16, curRelCoor.z + 16);
                }

                if(tipBranchLength >= dist_thresh)
                    break;
            }

            if(tipBranchLength >= dist_thresh || !flag)
                break;
            float xLabel = seg.row[indexs[size-1]].x;
            float yLabel = seg.row[indexs[size-1]].y;
            float zLabel = seg.row[indexs[size-1]].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(wholeGrid2segIDmap[gridKey].size()!=2)
            {
                tipBranchLength = 0;
                break;
            }
            for(auto segIt = wholeGrid2segIDmap[gridKey].begin(); segIt != wholeGrid2segIDmap[gridKey].end(); segIt++){
                if(segId != *segIt)
                {
                    segId = *segIt;
                    break;
                }
            }

            if(find(visitedSegIds.begin(),visitedSegIds.end(),segId) == visitedSegIds.end())
                visitedSegIds.push_back(segId);
            else
            {
                tipBranchLength = 0;
                break;
            }
            seg = inputSegList.seg[segId];
            float xLabel2 = seg.row[0].x;
            float yLabel2 = seg.row[0].y;
            float zLabel2 = seg.row[0].z;
            QString gridKeyQ2 = QString::number(xLabel2) + "_" + QString::number(yLabel2) + "_" + QString::number(zLabel2);
            string gridKey2 = gridKeyQ2.toStdString();
            if(gridKey2 != gridKey)
                isReverse = true;
            else
                isReverse = false;
        }

        if(tipBranchLength >= dist_thresh){
            NeuronSWC s;
            stringToXYZ(*it, s.x, s.y, s.z);
            s.type = 10;
            if(s.x > 33 && s.x + 33 <maxRes.x && s.y > 33 && s.y + 33 < maxRes.y && s.z > 33 && s.z + 33 < maxRes.z)
            {
                QString qKey = QString::number(s.x) + "_" + QString::number(s.y) + "_" + QString::number(s.z);
                string key = qKey.toStdString();
                if(detectedTipPoints.find(key) == detectedTipPoints.end())
                {
                    detectedTipPoints.insert(key);
                    outputSpecialPoints.push_back(s);
                    tipInfoMap[QString::fromStdString(*it)] = segData;
                }
            }
        }
    }

    if(!removeFlag)
        return outputSpecialPoints;

    vector<V_NeuronSWC> delSegVec;
    for(auto it=outputSpecialPoints.begin(); it!=outputSpecialPoints.end(); it++){
        float xLabel = it->x;
        float yLabel = it->y;
        float zLabel = it->z;
        QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
        string gridKey = gridKeyQ.toStdString();

        size_t segId=*wholeGrid2segIDmap[gridKey].begin();

        V_NeuronSWC& seg = inputSegList.seg[segId];

        float xLabel0 = seg.row[0].x;
        float yLabel0 = seg.row[0].y;
        float zLabel0 = seg.row[0].z;
        QString gridKeyQ0 = QString::number(xLabel0) + "_" + QString::number(yLabel0) + "_" + QString::number(zLabel0);
        string gridKey0 = gridKeyQ0.toStdString();

        bool isReverse=false;
        if(wholeGrid2segIDmap[gridKey0].size()!=1)
        {
            isReverse=true;
        }

        int size=seg.row.size();
        vector<int> indexs(size);
        for(int m=0;m<size;m++)
            indexs[m]=m;
        if(isReverse)
            reverse(indexs.begin(),indexs.end());

        if(size<=7){
            auto segIt=findseg(inputSegList.seg.begin(),inputSegList.seg.end(),seg);
            if(segIt!=inputSegList.seg.end())
            {
                delSegVec.push_back(seg);
            }
            continue;
        }

        qDebug()<<"before: ";
        seg.printInfo();

        int count=1;
        while(count<=6){
            auto tmpIt = seg.row.end();
            if(!isReverse){
                tmpIt=seg.row.begin();
            }
            else{
                tmpIt=seg.row.end()-1;
            }
            seg.row.erase(tmpIt);
            count++;
        }

        int nodeNo = 1;
        for (vector<V_NeuronSWC_unit>::iterator it_unit = seg.row.begin();
             it_unit != seg.row.end(); it_unit++)
        {
            it_unit->data[0] = nodeNo;
            it_unit->data[6] = nodeNo + 1;
            ++nodeNo;
        }
        (seg.row.end()-1)->data[6]=-1;
        qDebug()<<"after: ";
        seg.printInfo();
    }

    for(auto delIt=delSegVec.begin(); delIt!=delSegVec.end(); delIt++){
        auto segIt=findseg(inputSegList.seg.begin(),inputSegList.seg.end(),*delIt);
        if(segIt!=inputSegList.seg.end())
        {
            inputSegList.seg.erase(segIt);
        }
    }

    return outputSpecialPoints;
}

vector<NeuronSWC> CollDetection::branchingDetection(V_NeuronSWC_list inputSegList, double dist_thresh){
    vector<NeuronSWC> outputSpecialPoints;
    if(inputSegList.seg.size()==0)
        return outputSpecialPoints;

    if(maxRes.x==0 && maxRes.y==0 && maxRes.z==0)
        getImageMaxRES();
    map<string, bool> isEndPointMap;
    map<string, set<size_t> > wholeGrid2segIDmap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];

        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            wholeGrid2segIDmap[gridKey].insert(size_t(i));

            if(j == 0 || j == seg.row.size() - 1){
                isEndPointMap[gridKey] = true;
            }
        }
    }

    //末端点和分叉点
    vector<string> points;
    vector<set<int>> linksIndex;
    //    vector<vector<int>> linksIndexVec;
    map<string,int> pointsIndexMap;

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(j==0 || j==seg.row.size()-1){
                //在pointsIndexMap中找不到某个线的末端点
                if(pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }else{
                if(wholeGrid2segIDmap[gridKey].size()>1 &&
                    isEndPointMap.find(gridKey) != isEndPointMap.end() &&
                    pointsIndexMap.find(gridKey) == pointsIndexMap.end()){
                    points.push_back(gridKey);
                    linksIndex.push_back(set<int>());
                    //                    linksIndexVec.push_back(vector<int>());
                    pointsIndexMap[gridKey] = points.size() - 1;
                }
            }
        }
    }

    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        V_NeuronSWC seg = inputSegList.seg[i];
        vector<int> segIndexs;
        set<int> segIndexsSet;
        segIndexs.clear();
        segIndexsSet.clear();
        for(size_t j=0; j<seg.row.size(); ++j){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            if(pointsIndexMap.find(gridKey) != pointsIndexMap.end()){
                int index = pointsIndexMap[gridKey];
                if(segIndexsSet.find(index) == segIndexsSet.end()){
                    segIndexs.push_back(index);
                    segIndexsSet.insert(index);
                }
            }
        }
        //        qDebug()<<"i : "<<i<<"seg size: "<<seg.row.size()<<" segIndexsSize: "<<segIndexs.size();
        for(size_t j=0; j<segIndexs.size()-1; ++j){
            if(segIndexs[j] == 1 || segIndexs[j+1] == 1){
                qDebug()<<segIndexs[j]<<" "<<segIndexs[j+1];
            }
            linksIndex[segIndexs[j]].insert(segIndexs[j+1]);
            //            linksIndexVec[segIndexs[j]].push_back(segIndexs[j+1]);
            linksIndex[segIndexs[j+1]].insert(segIndexs[j]);
            //            linksIndexVec[segIndexs[j+1]].push_back(segIndexs[j]);
        }
    }

    for(size_t i=0; i<points.size(); ++i){
        //        qDebug()<<i<<" link size: "<<linksIndex[i].size();
        if(linksIndex[i].size() == 3){
            bool isValid = true;
            auto connectedSegsSet = wholeGrid2segIDmap[points[i]];
            if(connectedSegsSet.size() == 3){
                for(auto segIt=connectedSegsSet.begin(); segIt!=connectedSegsSet.end(); segIt++){
                    double length = getSegLength(inputSegList.seg[*segIt]);
                    float xLabel = inputSegList.seg[*segIt].row[0].x;
                    float yLabel = inputSegList.seg[*segIt].row[0].y;
                    float zLabel = inputSegList.seg[*segIt].row[0].z;
                    QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                    string gridKey = gridKeyQ.toStdString();
                    if(gridKey==points[i]){
                        xLabel = inputSegList.seg[*segIt].row[inputSegList.seg[*segIt].nrows()-1].x;
                        yLabel = inputSegList.seg[*segIt].row[inputSegList.seg[*segIt].nrows()-1].y;
                        zLabel = inputSegList.seg[*segIt].row[inputSegList.seg[*segIt].nrows()-1].z;
                        gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                        gridKey = gridKeyQ.toStdString();
                    }
                    if(length<dist_thresh && wholeGrid2segIDmap[gridKey].size()==1)
                    {
                        isValid = false;
                        break;
                    }
                }
            }
            else if(connectedSegsSet.size() == 2){
                int wholeSegIndex = -1;
                int specialSegIndex = -1;
                int middleIndex = -1;
                for(auto segIt=connectedSegsSet.begin(); segIt!=connectedSegsSet.end(); segIt++){
                    int index = getPointInSegIndex(points[i], inputSegList.seg[*segIt]);
                    if(index == -1){
                        isValid = false;
                        break;
                    }
                    else if(index==0 || index==inputSegList.seg[*segIt].nrows()-1){
                        wholeSegIndex = *segIt;
                    }
                    else if(index>0 && index<inputSegList.seg[*segIt].nrows()-1){
                        specialSegIndex = *segIt;
                        middleIndex = index;
                    }
                }
                if(!isValid || wholeSegIndex==-1 || specialSegIndex==-1)
                    continue;

                double length1 = getSegLength(inputSegList.seg[wholeSegIndex]);
                double length2 = getSegLengthBetweenIndexs(inputSegList.seg[specialSegIndex], 0, middleIndex);
                double length3 = getSegLengthBetweenIndexs(inputSegList.seg[specialSegIndex], middleIndex, inputSegList.seg[specialSegIndex].nrows()-1);

                float xLabel = inputSegList.seg[wholeSegIndex].row[0].x;
                float yLabel = inputSegList.seg[wholeSegIndex].row[0].y;
                float zLabel = inputSegList.seg[wholeSegIndex].row[0].z;
                QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                string gridKey = gridKeyQ.toStdString();
                if(gridKey==points[i]){
                    xLabel = inputSegList.seg[wholeSegIndex].row[inputSegList.seg[wholeSegIndex].nrows()-1].x;
                    yLabel = inputSegList.seg[wholeSegIndex].row[inputSegList.seg[wholeSegIndex].nrows()-1].y;
                    zLabel = inputSegList.seg[wholeSegIndex].row[inputSegList.seg[wholeSegIndex].nrows()-1].z;
                    gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                    gridKey = gridKeyQ.toStdString();
                }
                if(length1<dist_thresh && wholeGrid2segIDmap[gridKey].size()==1)
                    continue;

                xLabel = inputSegList.seg[specialSegIndex].row[0].x;
                yLabel = inputSegList.seg[specialSegIndex].row[0].y;
                zLabel = inputSegList.seg[specialSegIndex].row[0].z;
                gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                gridKey = gridKeyQ.toStdString();
                if(length2<dist_thresh && wholeGrid2segIDmap[gridKey].size()==1)
                    continue;

                xLabel = inputSegList.seg[specialSegIndex].row[inputSegList.seg[specialSegIndex].nrows()-1].x;
                yLabel = inputSegList.seg[specialSegIndex].row[inputSegList.seg[specialSegIndex].nrows()-1].y;
                zLabel = inputSegList.seg[specialSegIndex].row[inputSegList.seg[specialSegIndex].nrows()-1].z;
                gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
                gridKey = gridKeyQ.toStdString();
                if(length3<dist_thresh && wholeGrid2segIDmap[gridKey].size()==1)
                    continue;
            }

            if(!isValid)
                continue;

            NeuronSWC s;
            stringToXYZ(points[i],s.x,s.y,s.z);
            s.type = 7;
            if(s.x>65&&s.x+65<maxRes.x&&s.y>65&&s.y+65<maxRes.y&&s.z>65&&s.z+65<maxRes.z)
            {
                QString qKey = QString::number(s.x) + "_" + QString::number(s.y) + "_" + QString::number(s.z);
                string key = qKey.toStdString();
                if(detectedBranchingPoints.find(key) == detectedBranchingPoints.end())
                {
                    detectedBranchingPoints.insert(key);
                    outputSpecialPoints.push_back(s);
                }
            }
        }
    }
    return outputSpecialPoints;
}

QJsonArray CollDetection::crossingDetection(){
    QString swcFileName=myServer->getAnoName()+".ano.eswc";
    QString fileSavePath=myServer->swcpath.left(myServer->swcpath.size()-QString(".ano.eswc").size())+"_forcrossing.ano.eswc";
    writeESWC_file(fileSavePath,V_NeuronSWC_list__2__NeuronTree(myServer->segments));

    QFile *file = new QFile(fileSavePath);
    file->open(QIODevice::Text|QIODevice::ReadWrite);
    file->setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    sortSWC(fileSavePath,fileSavePath,0);
    setSWCRadius(fileSavePath,1);

    file->close();

    std::filesystem::path swcPath(fileSavePath.toStdString());

    std::vector<NeuronUnit> neurons;
    std::filesystem::path fullPath = swcPath;
    ESwc swc(fullPath.string());
    neurons = swc.getNeuron();

    std::cout << "FilePath:"<<fullPath.string();

    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<util::Node> nodes;
    std::vector<int> rootNodeIds;

    std::vector<CrossingDetect::KeyPointsType> keypointsList;
    std::vector<CrossingDetect::BranchesType> selectedBranchesList;
    std::vector<util::ImageResolutionInfo> imageResolutionInfoList;

    int curNode = 0;
    for (auto &neuron: neurons) {
        nodes.push_back({.n=neuron.n, .parent=neuron.parent, .x=neuron.x, .y=neuron.y, .z=neuron.z});
        if (neuron.parent == -1) {
            rootNodeIds.push_back(neuron.n);
        }
        curNode++;

        if ((rootNodeIds.size() == 2) || (rootNodeIds.size() == 1 && neurons.size() == curNode)) {

            util::Node lastNode{};
            int lastRootNodeId = -1;
            bool multiRoot = false;
            if (rootNodeIds.size() == 2) {
                multiRoot = true;
            }

            if (multiRoot) {
                lastNode = nodes.back();
                lastRootNodeId = rootNodeIds.back();
                nodes.pop_back();
                rootNodeIds.pop_back();
            }

            CrossingDetect instance;
            instance.initializeNodeData(nodes, rootNodeIds);
            instance.generateBranches();
            instance.selectBranches();
            instance.generateNearestKeyPoint();
            instance.removeFromKeyPoint();

            auto &keyPoints = instance.getKeyPoint();
            auto &selectedBranch = instance.getSelectedBranch();

            if(maxRes.x==0 && maxRes.y==0 && maxRes.z==0)
                getImageMaxRES();
            auto edgeThreshold = ConfigManager::getInstance().edgePointIgnoreThreshold;
            for (auto &point: keyPoints) {
                if (maxRes.x > edgeThreshold * 2
                    && maxRes.y > edgeThreshold * 2
                    && maxRes.z > edgeThreshold * 2) {
                    auto isXConstrictionOK = point.first.first.first.x >= edgeThreshold &&
                                             point.first.first.first.x <= maxRes.x - edgeThreshold;
                    auto isYConstrictionOK = point.first.first.first.y >= edgeThreshold &&
                                             point.first.first.first.y <= maxRes.y - edgeThreshold;
                    auto isZConstrictionOK = point.first.first.first.z >= edgeThreshold &&
                                             point.first.first.first.z <= maxRes.z - edgeThreshold;

                    if (!(isXConstrictionOK && isYConstrictionOK && isZConstrictionOK)) {
                        point.second = false;
                        continue;
                    }
                    QString qKey1 = QString::number(point.first.first.first.x) + "_" + QString::number(point.first.first.first.y) + "_" + QString::number(point.first.first.first.z);
                    string key1 = qKey1.toStdString();
                    QString qKey2 = QString::number(point.first.second.first.x) + "_" + QString::number(point.first.second.first.y) + "_" + QString::number(point.first.second.first.z);
                    string key2 = qKey2.toStdString();
                    set<string> tmpSet;
                    tmpSet.insert(key1);
                    tmpSet.insert(key2);
                    if(detectedCrossingPoints.find(tmpSet) == detectedCrossingPoints.end())
                    {
                        detectedCrossingPoints.insert(tmpSet);
                    }else{
                        point.second = false;
                    }
                }
            }

            keypointsList.push_back(keyPoints);
            selectedBranchesList.push_back(selectedBranch);

            // clear data before processing more root nodes
            nodes.clear();
            rootNodeIds.clear();

            if (multiRoot) {
                nodes.push_back(lastNode);
                rootNodeIds.push_back(lastRootNodeId);
            }
        }
    }

    ResultWriter writer("", "");
    QJsonArray infos = writer.getData(keypointsList, selectedBranchesList);
    updateCrossingInfoMap(infos);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = endTime - startTime;

    std::cout << "\n";
    std::cout << "Using time:"<<std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()<<"ms\n";
    std::cout << "--------------------------\n";
    return infos;
}

void CollDetection::updateCrossingInfoMap(QJsonArray infos){
    map<string, set<size_t>> wholeGrid2SegIDMap = getWholeGrid2SegIDMap(myServer->segments);
    for (auto info : infos) {
        QString centerCoorStrRep;
        pair<pair<QString, int>, pair<QString, int>> coor2SegIndexPair;
        pair<pair<QString, pair<int, int>>, pair<QString, pair<int, int>>> coor2RowIndexRangePair;
        QJsonArray infoJA = info.toArray();
        for (int i = 0; i < infoJA.size(); i++) {
            auto allPointInfo = infoJA.at(i);
            QJsonObject allPOintInfoJO = allPointInfo.toObject();
            float x = allPOintInfoJO.value("x").toDouble();
            float y = allPOintInfoJO.value("y").toDouble();
            float z = allPOintInfoJO.value("z").toDouble();
            QString centerCoorStr = QString::number(x) + "_" + QString::number(y) + "_" + QString::number(z);
            if (i == 0) {
                centerCoorStrRep = centerCoorStr;
                crossingInfoMap[centerCoorStrRep] = CrossingInfo();
            }

            set<size_t> oneSegIDSet;
            string parentBeginCoorStdStr, offspringEndCoorStdStr;
            QJsonArray parentsCoorsJA = allPOintInfoJO.value("parentsCoors").toArray();
            QJsonArray offspringsCoorsJA = allPOintInfoJO.value("offspringsCoors").toArray();
            for (auto parentCoor : parentsCoorsJA) {
                QJsonObject parentCoorJO = parentCoor.toObject();
                float coor_x = parentCoorJO.value("x").toDouble();
                float coor_y = parentCoorJO.value("y").toDouble();
                float coor_z = parentCoorJO.value("z").toDouble();
                QString coorStr = QString::number(coor_x) + "_" + QString::number(coor_y) + "_" + QString::number(coor_z);
                string coorStdStr = coorStr.toStdString();
                set<size_t> tmpSegIDSet = wholeGrid2SegIDMap[coorStdStr];
                for (auto it = tmpSegIDSet.begin(); it != tmpSegIDSet.end(); it++){
                    oneSegIDSet.insert(*it);
                }
            }
            for (auto offspringCoor : offspringsCoorsJA) {
                QJsonObject offspringCoorJO = offspringCoor.toObject();
                float coor_x = offspringCoorJO.value("x").toDouble();
                float coor_y = offspringCoorJO.value("y").toDouble();
                float coor_z = offspringCoorJO.value("z").toDouble();
                QString coorStr = QString::number(coor_x) + "_" + QString::number(coor_y) + "_" + QString::number(coor_z);
                string coorStdStr = coorStr.toStdString();
                set<size_t> tmpSegIDSet = wholeGrid2SegIDMap[coorStdStr];
                for (auto it = tmpSegIDSet.begin(); it != tmpSegIDSet.end(); it++){
                    oneSegIDSet.insert(*it);
                }
            }

            if (oneSegIDSet.size() != 1) {
                crossingInfoMap[centerCoorStrRep].isAbleCorrect = false;
                break;
            }

            QJsonObject parentBeginJO = parentsCoorsJA.at(0).toObject();
            float parentX = parentBeginJO.value("x").toDouble();
            float parentY = parentBeginJO.value("y").toDouble();
            float parentZ = parentBeginJO.value("z").toDouble();
            parentBeginCoorStdStr = (QString::number(parentX) + "_" + QString::number(parentY) + "_" + QString::number(parentZ)).toStdString();
            QJsonObject offspringEndJO = offspringsCoorsJA.at(offspringsCoorsJA.size() - 1).toObject();
            float offspringX = offspringEndJO.value("x").toDouble();
            float offspringY = offspringEndJO.value("y").toDouble();
            float offspringZ = offspringEndJO.value("z").toDouble();
            offspringEndCoorStdStr = (QString::number(offspringX) + "_" + QString::number(offspringY) + "_" + QString::number(offspringZ)).toStdString();
            int minIndex = getPointInSegIndex(offspringEndCoorStdStr, myServer->segments.seg[*oneSegIDSet.begin()]);
            int maxIndex = getPointInSegIndex(parentBeginCoorStdStr, myServer->segments.seg[*oneSegIDSet.begin()]);

            if (i == 0) {
                coor2SegIndexPair.first = make_pair(centerCoorStr, *oneSegIDSet.begin());
                coor2RowIndexRangePair.first = make_pair(centerCoorStr, make_pair(minIndex, maxIndex));
            }
            if (i == 1) {
                coor2SegIndexPair.second = make_pair(centerCoorStr, *oneSegIDSet.begin());
                coor2RowIndexRangePair.second = make_pair(centerCoorStr, make_pair(minIndex, maxIndex));
            }

        }
        crossingInfoMap[centerCoorStrRep].coor2SegIndexPair = coor2SegIndexPair;
        crossingInfoMap[centerCoorStrRep].coor2RowIndexRangePair = coor2RowIndexRangePair;
    }
}

void CollDetection::handleMulFurcation(vector<NeuronSWC>& outputSpecialPoints, int& count, double dist_thre){
    QString tobeSendMsg=QString("/WARN_MulBifurcation:server,");
    QStringList result;
    QString comment = "Multifurcation";

    for(int i=0;i<outputSpecialPoints.size();i++){
        RGB8 color = getColorFromType(outputSpecialPoints[i].type);
        if(myServer->isSomaExists)
        {
            if(distance(outputSpecialPoints[i].x, myServer->somaCoordinate.x, outputSpecialPoints[i].y, myServer->somaCoordinate.y,
                         outputSpecialPoints[i].z, myServer->somaCoordinate.z) > dist_thre)
            {
                QString curMarker=QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(outputSpecialPoints[i].x).arg(outputSpecialPoints[i].y).arg(outputSpecialPoints[i].z);

                QString msg=QString("/WARN_MulBifurcation:server,"+curMarker);
                bool isSucess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_MulBifurcation:").size()), comment);
                if(isSucess){
                    result.push_back(curMarker);
                    count++;
                }
            }
        }

        else{
            QString curMarker=QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(outputSpecialPoints[i].x).arg(outputSpecialPoints[i].y).arg(outputSpecialPoints[i].z);

            QString msg=QString("/WARN_MulBifurcation:server,"+curMarker);
            bool isSucess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_MulBifurcation:").size()), comment);
            if(isSucess){
                result.push_back(curMarker);
                count++;
            }
        }
    }

    tobeSendMsg = tobeSendMsg + result.join(",");

    if(count!=0)
        emit myServer->clientSendMsgs({tobeSendMsg});

    qDebug()<<"Server finish /WARN_MulBifurcation";
}

void CollDetection::handleLoop(vector<NeuronSWC>& outputSpecialPoints, int& count){
    QString tobeSendMsg=QString("/WARN_Loop:server 0,");
    QStringList result;
    QString comment = "Loop";

    for(int i=0;i<outputSpecialPoints.size();i++){
        RGB8 color = getColorFromType(outputSpecialPoints[i].type);
        QString curMarker=QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(outputSpecialPoints[i].x).arg(outputSpecialPoints[i].y).arg(outputSpecialPoints[i].z);

        QString msg=QString("/WARN_Loop:server,"+curMarker);
        bool isSucess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_Loop:").size()), comment);
        if(isSucess){
            result.push_back(curMarker);
            count++;
        }
    }

    tobeSendMsg = tobeSendMsg + result.join(",");

    if(count!=0)
        emit myServer->clientSendMsgs({tobeSendMsg});
    else{
        tobeSendMsg = QString("/WARN_Loop:server 1");
        emit myServer->clientSendMsgs({tobeSendMsg});
    }
    qDebug()<<"Server finish /WARN_Loop";
}

void CollDetection::handleNearBifurcation(vector<NeuronSWC>& bifurPoints, int& count){
    count+=bifurPoints.size();
    QString comment="Approaching bifurcation";
    for(int i=0;i<bifurPoints.size();i++){
        RGB8 color = getColorFromType(bifurPoints[i].type);
        QStringList result;
        result.push_back(QString("server"));
        result.push_back(QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(bifurPoints[i].x).arg(bifurPoints[i].y).arg(bifurPoints[i].z));
        QString msg=QString("/WARN_NearBifurcation:"+result.join(","));
        //            const std::string data=msg.toStdString();
        //            const std::string header=QString("DataTypeWithSize:%1 %2\n").arg(0).arg(data.size()).toStdString();
        //            auto sockets=hashmap.values();
        //        emit clientAddMarker(msg.trimmed().right(msg.size()-QString("/WARN_Crossing:").size()));
        bool isSuccess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_NearBifurcation:").size()), comment);
        if(isSuccess)
            emit myServer->clientSendMsgs({msg});
    }
    qDebug()<<"Server finish /WARN_NearBifurcation";

}

void CollDetection::handleTip(vector<NeuronSWC>& tipPoints){
//    for(int i=0;i<tipPoints.size();i++){
//        QStringList result;
//        result.push_back(QString("%1 server").arg(0));
//        result.push_back(QString("%1 %2 %3 %4").arg(tipPoints[i].type).arg(tipPoints[i].x).arg(tipPoints[i].y).arg(tipPoints[i].z));
//        QString msg=QString("/WARN_Tip:"+result.join(","));
//        myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_Tip:").size()));
//        qDebug()<<"Server finish /WARN_Tip";
//        emit myServer->clientSendMsgs({msg});
//    }

//    for(auto it=tipPoints.begin(); it!=tipPoints.end(); it++){
//        qDebug()<<it->x<<" "<<it->y<<" "<<it->z;
//    }

    if(tipPoints.size()!=0){
        QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart filePart;
        QString swcFileName=myServer->getAnoName()+".ano.eswc";
        QString fileSavePath=myServer->swcpath.left(myServer->swcpath.size()-QString(".ano.eswc").size())+"_fortip.ano.eswc";
        writeESWC_file(fileSavePath,V_NeuronSWC_list__2__NeuronTree(myServer->segments));

        sortSWC(fileSavePath,fileSavePath,0);
        setSWCRadius(fileSavePath,1);

        // 创建一个QFile对象，用于读取要上传的文件
        QFile *file = new QFile(fileSavePath);
        file->open(QIODevice::Text|QIODevice::ReadWrite);
        file->setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);

        filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("text/plain"));
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"swcFile\"; filename=\""+ swcFileName + "\"")); // file为后端定义的key，filename即为excel文件名
        filePart.setBodyDevice(file);
        file->setParent(multiPart); // 文件将由multiPart对象进行管理

        multiPart->append(filePart);
        QNetworkRequest fileRequest(QUrl(SuperUserHostAddress+"/detect/file/for-missing"));
        // 发送HTTP POST请求
        QNetworkReply* fileReply = accessManager->post(fileRequest, multiPart);
        multiPart->setParent(fileReply); // reply对象将负责删除multiPart对象
        QEventLoop tmpeventLoop;
        connect(fileReply, &QNetworkReply::finished, &tmpeventLoop, &QEventLoop::quit);
        tmpeventLoop.exec(QEventLoop::ExcludeUserInputEvents);

        if(fileReply->error())
        {
            qDebug() << "SENDFILEERROR!";
            qDebug() << fileReply->errorString();
        }
        int fileResCode=fileReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug()<<"sendFile"<<fileResCode;
        //清理资源
        QFile::remove(fileSavePath);
        file->close();
        fileReply->deleteLater();

        QJsonObject json;
        QString obj=myServer->getImage();
        json.insert("obj",obj);
//        json.insert("res", myServer->RES);
        json.insert("swcNameWithNoSuffix", myServer->getAnoName());
        QJsonArray coorList;
        for(int i=0; i<tipPoints.size();i++){
            QJsonObject coor;
            coor.insert("x", tipPoints[i].x);
            coor.insert("y", tipPoints[i].y);
            coor.insert("z", tipPoints[i].z);
            coorList.append(coor);
        }
        json.insert("coors",coorList);

        QJsonDocument document;
        document.setObject(json);
        QString str=QString(document.toJson());
        QByteArray byteArray=str.toUtf8();

        // 创建一个QNetworkRequest对象，设置URL和请求方法
        QNetworkRequest request(QUrl(SuperUserHostAddress+"/detect/missing"));
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        //        request.setRawHeader("Content-Type", "multipart/form-data; boundary=" + multiPart->boundary());

        // 发送HTTP POST请求
        QNetworkReply* reply = accessManager->post(request, byteArray);

        QEventLoop eventLoop;
        connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

        if(reply->error())
        {
            qDebug() << "ERROR!";
            qDebug() << reply->errorString();
        }
        int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug()<<"handleTip"<<code;
        QByteArray responseData = reply->readAll();
        vector<NeuronSWC> markPoints;
        vector<TipCoorPredictedResult> coorResults;
        QString relPath;
        if(code==200)
        {
            //解析json
            QJsonParseError json_error;
            QJsonDocument doucment = QJsonDocument::fromJson(responseData, &json_error);
            if (json_error.error == QJsonParseError::NoError) {
                if (doucment.isObject()) {
                    const QJsonObject obj = doucment.object();
                    qDebug() << obj;
                    QString objCode;
                    QString objMsg;
                    if (obj.contains("code")) {
                        QJsonValue value = obj.value("code");
                        if (value.isString()) {
                            objCode= value.toString();
                            qDebug() << "code : " << objCode;
                        }
                    }
                    if (obj.contains("msg")) {
                        QJsonValue value = obj.value("msg");
                        if (value.isString()) {
                            QString objMsg = value.toString();
                            qDebug() << "msg : " << objMsg;
                        }
                    }
                    if (obj.contains("data")&&objCode=="200") {
                        QJsonValue value = obj.value("data");
                        if (value.isObject()) {  // Version 的 value 是数组
                            QJsonObject resData = value.toObject();
                            if (resData.contains("relPath")){
                                relPath = resData.value("relPath").toString();
                            }
                            if (resData.contains("coorPredictedResultList")){
                                QJsonArray coorPredictedResList = resData.value("coorPredictedResultList").toArray();
                                for (int i = 0; i < coorPredictedResList.size(); i++){
                                    TipCoorPredictedResult coorResult;
                                    QJsonObject coorResultObj = coorPredictedResList.at(i).toObject();
                                    QJsonObject maxResCoorObj = coorResultObj.value("maxResCoor").toObject();
                                    QString storeDirName = coorResultObj.value("storeDirName").toString();
                                    int y_pred = coorResultObj.value("y_pred").toInt();
                                    coorResult.maxResCoor = XYZ(maxResCoorObj.value("x").toDouble(), maxResCoorObj.value("y").toDouble(),
                                                                maxResCoorObj.value("z").toDouble());
                                    coorResult.storeDirName = storeDirName;
                                    coorResult.y_pred = y_pred;
                                    coorResults.push_back(coorResult);
                                }
                            }
                            for (auto it = coorResults.begin(); it != coorResults.end(); it++){
                                if (it->y_pred == 1){
                                    NeuronSWC s;
                                    s.x = it->maxResCoor.x;
                                    s.y = it->maxResCoor.y;
                                    s.z = it->maxResCoor.z;
                                    s.type=10;
                                    markPoints.push_back(s);
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            std::cerr<<"handle tip error!";
        }

        if(isAutoCorrect){
            qint64 preTimeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
            vector<TipCoorPredictedResult> missingPart = getMissingPart(coorResults);
            vector<QString> coorList;
            map<QString, MissingForSegData> segDataMap;

            for(auto it = missingPart.begin(); it != missingPart.end(); it++){
                XYZ maxResCoor = it->maxResCoor;
                QString maxResCoorStr = QString::number(maxResCoor.x) + "_" + QString::number(maxResCoor.y) + "_" + QString::number(maxResCoor.z);
                MissingForSegData segData = tipInfoMap[maxResCoorStr];
                segData.storeDirName = it->storeDirName;
                segDataMap[maxResCoorStr] = segData;
                coorList.push_back(it->storeDirName);
            }

            QString result_relpath = "";
            if (requestForSeg(relPath, coorList, result_relpath)){
                qint64 preTimeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
                getApp2TracingResult(segDataMap, result_relpath);
                qint64 postTimeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
                float consumedTime = (postTimeStamp - preTimeStamp) / 1000.0;
                // 使用 QString 进行格式化
                QString consumedTime_ = QString::number(consumedTime, 'f', 3);
                qDebug() << "consumed_time_app2: " << consumedTime_;

                fliterTip(segDataMap);
                myServer->mutex.lock();
                autoCorrectMissing(segDataMap);
                myServer->mutex.unlock();
            }

            // 清理资源
            reply->deleteLater();
            tipInfoMap.clear();

            qint64 postTimeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
            float consumedTime = (postTimeStamp - preTimeStamp) / 1000.0;
            // 使用 QString 进行格式化
            QString consumedTime_ = QString::number(consumedTime, 'f', 3);
            qDebug() << "consumed_time_sum: " << consumedTime_;
            return;
        }

        QString tobeSendMsg=QString("/WARN_TipUndone:server,");
        QStringList result;
        QString comment = "Missing";
        int count = 0;
        filterTip(markPoints);

        for(int i=0;i<markPoints.size();i++){
            RGB8 color = getColorFromType(markPoints[i].type);
            QString curMarker=QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(markPoints[i].x).arg(markPoints[i].y).arg(markPoints[i].z);

            QString msg=QString("/WARN_TipUndone:server,"+curMarker);
            bool isSucess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_TipUndone:").size()), comment);
            if(isSucess){
                result.push_back(curMarker);
                count++;
            }
        }

        tobeSendMsg = tobeSendMsg + result.join(",");

        if(count!=0)
            emit myServer->clientSendMsgs({tobeSendMsg});

        qDebug()<<"Server finish /WARN_TipUndone";

//        if(markPoints.size()!=0)
//            myServer->imediateSave();

        //清理资源
        reply->deleteLater();     
    }
}

void CollDetection::filterTip(vector<NeuronSWC>& markPoints){
    map<string, set<size_t>> wholeGrid2SegIDMap = getWholeGrid2SegIDMap(myServer->segments);

    auto iter = markPoints.begin();
    while(iter != markPoints.end()){
        float xLabel = iter->x;
        float yLabel = iter->y;
        float zLabel = iter->z;
        QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
        string gridKey = gridKeyQ.toStdString();
        if(wholeGrid2SegIDMap.find(gridKey) == wholeGrid2SegIDMap.end() || wholeGrid2SegIDMap[gridKey].size() != 1){
            iter = markPoints.erase(iter);
        }else{
            iter++;
        }
    }
}

void CollDetection::fliterTip(map<QString, MissingForSegData>& segDataMap){
    map<string, set<size_t>> wholeGrid2SegIDMap = getWholeGrid2SegIDMap(myServer->segments);

    auto iter = segDataMap.begin();
    while(iter != segDataMap.end()){
        QString coorStr = iter->first;
        XYZ coor = qstringToXYZ(coorStr);
        float xLabel = coor.x;
        float yLabel = coor.y;
        float zLabel = coor.z;
        QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
        string gridKey = gridKeyQ.toStdString();
        if(wholeGrid2SegIDMap.find(gridKey) == wholeGrid2SegIDMap.end() || wholeGrid2SegIDMap[gridKey].size() != 1){
            iter = segDataMap.erase(iter);
        }else{
            iter++;
        }
    }
}

void CollDetection::fliterCrossing(){
    map<string, set<size_t>> wholeGrid2SegIDMap = getWholeGrid2SegIDMap(myServer->segments);

    auto iter = crossingInfoMap.begin();
    while(iter != crossingInfoMap.end()){
        QString coorStr = iter->first;
        XYZ coor = qstringToXYZ(coorStr);
        float xLabel = coor.x;
        float yLabel = coor.y;
        float zLabel = coor.z;
        QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
        string gridKey = gridKeyQ.toStdString();
        if(wholeGrid2SegIDMap.find(gridKey) == wholeGrid2SegIDMap.end() || wholeGrid2SegIDMap[gridKey].size() != 1){
            iter = crossingInfoMap.erase(iter);
        }else{
            iter++;
        }
    }
}

vector<CollDetection::TipCoorPredictedResult> CollDetection::getMissingPart(vector<TipCoorPredictedResult> tipCoorResults){
    vector<TipCoorPredictedResult> missingPart;
    for (auto it = tipCoorResults.begin(); it != tipCoorResults.end(); it++){
        if (it->y_pred == 1){
            missingPart.push_back(*it);
        }
    }
    return missingPart;
}

bool CollDetection::requestForSeg(QString relPath, vector<QString> coorList, QString& result_relpath){
    QJsonObject json;
    QString image=myServer->getImage();
    QString methodName = "HumanNeuronFiberSegment";
    if (image.startsWith("17") || image.startsWith("18") || image.startsWith("19") || image.startsWith("pre")){
        methodName = "MouseNeuronFiberSegment";
    }
    json.insert("method_name", methodName);
    json.insert("swc_name", QString::fromStdString(myServer->swcName));
    QJsonArray coor_list;
    for(int i=0; i<coorList.size();i++){
        coor_list.append(coorList[i]);
    }
    json.insert("coor_list", coor_list);
    json.insert("relpath", relPath);

    QJsonDocument document;
    document.setObject(json);
    QString str = QString(document.toJson());
    QByteArray byteArray = str.toUtf8();

    // 创建一个QNetworkRequest对象，设置URL和请求方法
    QNetworkRequest request((QUrl(neuronFiberSegUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    //        request.setRawHeader("Content-Type", "multipart/form-data; boundary=" + multiPart->boundary());

    // 发送HTTP POST请求
    QNetworkReply* reply = accessManager->post(request, byteArray);

    QEventLoop eventLoop;
    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

    if (reply->error())
    {
        qDebug() << "ERROR!";
        qDebug() << reply->errorString();
    }
    int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug()<<"requestForSeg "<<code;
    QByteArray responseData = reply->readAll();

    if (code == 200){
        //解析json
        QJsonParseError json_error;
        QJsonDocument doucment = QJsonDocument::fromJson(responseData, &json_error);
        if (json_error.error == QJsonParseError::NoError) {
            if (doucment.isObject()){
                QJsonObject obj = doucment.object();
                qDebug() << obj;
                QJsonObject responseObj = obj.value("response").toObject();
                QString status = responseObj.value("status").toString();
                QString message = responseObj.value("message").toString();
                if (status != "ok"){
                    qDebug() << message;
                    return false;
                }
                else {
                    result_relpath = responseObj.value("result_relpath").toString();
                    return true;
                }
            }
        }
    }
}

void CollDetection::getApp2TracingResult(map<QString, MissingForSegData>& segDataMap, QString relpath){

    QString segImageDirPath = segDirPath + "/" + relpath;

    for (auto it = segDataMap.begin(); it != segDataMap.end(); it++){
        MissingForSegData segData = it->second;
        QString imagePath = segImageDirPath + "/" + it->second.storeDirName + ".v3dpbd";

        PARA_APP2 p = PARA_APP2();
        p.inimg_file = imagePath;
        LocationSimple ls;
        ls.x = segData.centerCoor.x + 1;
        ls.y = segData.centerCoor.y + 1;
        ls.z = segData.centerCoor.z + 1;
        p.landmarks.append(ls);
        ls.x = segData.edgeCoor.x + 1;
        ls.y = segData.edgeCoor.y + 1;
        ls.z = segData.edgeCoor.z + 1;
        p.landmarks.append(ls);

        if (proc_app2(p)){
            QString trace_swc_path = segImageDirPath + "/" + it->second.storeDirName + "_trace.swc";
            writeESWC_file(trace_swc_path, p.result);
            V_NeuronSWC_list trace_list = NeuronTree__2__V_NeuronSWC_list(p.result);
            if (trace_list.seg.size() < 2){
                continue;
            }
            float x_ratio, y_ratio, z_ratio;
            x_ratio = y_ratio = z_ratio = 2;
            XYZ centerCoorSubResGlobal = qstringToXYZ(it->second.storeDirName);
            XYZ startCoorSubRes = XYZ(centerCoorSubResGlobal.x - 16, centerCoorSubResGlobal.y - 16, centerCoorSubResGlobal.z - 16);
            vector<V_NeuronSWC> maxResSegVec = convertLocal2Global(trace_list, x_ratio, y_ratio, z_ratio, startCoorSubRes);

            XYZ centerCoorMaxResGlobal = convertLocal2Global(centerCoorSubResGlobal,  x_ratio, y_ratio, z_ratio, XYZ(0, 0, 0));
            XYZ edgeCoorMaxResGlobal = convertLocal2Global(segData.edgeCoor, x_ratio, y_ratio, z_ratio, startCoorSubRes);
            V_NeuronSWC addedSeg = extractAddedSeg(maxResSegVec, edgeCoorMaxResGlobal, edgeCoorMaxResGlobal);

            if (addedSeg.row.size() > 0){
                double dist1 = distance(addedSeg.row[0].x, edgeCoorMaxResGlobal.x, addedSeg.row[0].y, edgeCoorMaxResGlobal.y, addedSeg.row[0].z, edgeCoorMaxResGlobal.z);
                double dist2 = distance(addedSeg.row[addedSeg.row.size() - 1].x, edgeCoorMaxResGlobal.x, addedSeg.row[addedSeg.row.size() - 1].y, edgeCoorMaxResGlobal.y, addedSeg.row[addedSeg.row.size() - 1].z, edgeCoorMaxResGlobal.z);
                if (dist1 > dist2){
                    XYZ addedPoint = segData.maxResCoor;
                    V_NeuronSWC_unit unit;
                    unit.type = segData.type;
                    unit.r = 1;
                    unit.x = addedPoint.x;
                    unit.y = addedPoint.y;
                    unit.z = addedPoint.z;
                    unit.n = addedSeg.row.size() + 1;
                    unit.parent = -1;
                    addedSeg.row.push_back(unit);
                    addedSeg.row[addedSeg.row.size() - 2].parent = addedSeg.row[addedSeg.row.size() - 1].n;
                    for (auto rowIt = addedSeg.row.begin(); rowIt != addedSeg.row.end(); rowIt++){
                        //                    rowIt->type = segData.type;
                        rowIt->type = 5;
                    }
                    it->second.addedSeg = addedSeg;
                }
            }
        }
    }
}

vector<V_NeuronSWC> CollDetection::convertLocal2Global(V_NeuronSWC_list& inputSegList, float x_ratio, float y_ratio, float z_ratio, XYZ startCoor){
    vector<V_NeuronSWC> segVec;
    for (auto it = inputSegList.seg.begin(); it != inputSegList.seg.end(); it++){
        V_NeuronSWC seg = *it;
        for (auto rowIt = seg.row.begin(); rowIt != seg.row.end(); rowIt++){
            rowIt->x += startCoor.x;
            rowIt->y += startCoor.y;
            rowIt->z += startCoor.z;
            rowIt->x *= x_ratio;
            rowIt->y *= y_ratio;
            rowIt->z *= z_ratio;
        }
        segVec.push_back(seg);
    }
    return segVec;
}

XYZ CollDetection::convertLocal2Global(XYZ inputCoor, float x_ratio, float y_ratio, float z_ratio, XYZ startCoor){
    inputCoor.x += startCoor.x;
    inputCoor.y += startCoor.y;
    inputCoor.z += startCoor.z;
    inputCoor.x *= x_ratio;
    inputCoor.y *= y_ratio;
    inputCoor.z *= z_ratio;
    return inputCoor;
}

V_NeuronSWC CollDetection::extractAddedSeg(vector<V_NeuronSWC>& maxResSegVec, XYZ edgeCoorMaxResGlobal, XYZ centerCoorMaxResGlobal){
    double max_dist = 0;
    int index = -1;
    int rowIndex = -1;
    float x = edgeCoorMaxResGlobal.x;
    float y = edgeCoorMaxResGlobal.y;
    float z = edgeCoorMaxResGlobal.z;
    for (auto it = maxResSegVec.begin(); it != maxResSegVec.end(); it++){
        for (auto rowIt = it->row.begin(); rowIt != it->row.end(); rowIt++){
            double dist = distance(rowIt->x, x, rowIt->y, y, rowIt->z, z);
            if (dist > max_dist){
                max_dist = dist;
                index = it - maxResSegVec.begin();
                rowIndex = rowIt - it->row.begin();
            }
        }
    }

    XYZ maxDistCoor = maxResSegVec[index].row[rowIndex];
    if (distance(centerCoorMaxResGlobal.x, maxDistCoor.x, centerCoorMaxResGlobal.y, maxDistCoor.y, centerCoorMaxResGlobal.z, maxDistCoor.z) < 6){
        return V_NeuronSWC();
    }

    return maxResSegVec[index];
}

void CollDetection::autoCorrectMissing(map<QString, MissingForSegData>& segDataMap){
    QStringList msgList;

    for (auto it = segDataMap.begin(); it != segDataMap.end(); it++) {
        V3DLONG point_size = myServer->segments.nrows();
        proto::SwcDataV1 addSwcData;
        V_NeuronSWC seg = it->second.addedSeg;

        if (seg.row.size() == 0){
            continue;
        }

        for (int i=0; i<seg.row.size(); i++) {
            proto::SwcNodeInternalDataV1 swcNodeInternalData;
            swcNodeInternalData.set_n(point_size + i + 1);
            if(i == seg.row.size()-1)
                swcNodeInternalData.set_parent(-1);
            else
                swcNodeInternalData.set_parent(point_size + i + 2);
            swcNodeInternalData.set_x(seg.row[i].x);
            swcNodeInternalData.set_y(seg.row[i].y);
            swcNodeInternalData.set_z(seg.row[i].z);
            swcNodeInternalData.set_radius(seg.row[i].r);
            swcNodeInternalData.set_type(seg.row[i].type);
            swcNodeInternalData.set_mode(seg.row[i].creatmode);

            auto* newData = addSwcData.add_swcdata();
            newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
        }

        proto::CreateSwcNodeDataResponse response;
        if(!WrappedCall::addSwcNodeData(myServer->swcUuid, addSwcData, response, myServer->cachedUserData)){
            QString msg = "/WARN_AddSwcNodeDataError:server";
            emit myServer->clientSendMsgs({msg});
            return;
        }

        auto uuids = response.creatednodesuuid();
        for(int i=0; i<seg.row.size(); i++){
            seg.row[i].uuid = uuids.Get(i);
        }

        myServer->segments.append(seg);

        reverseSeg(seg);
        int firstSegID = -1;
        int secondSegID = -1;
        int index = myServer->segments.seg.size() - 1;
        for(int i = 0; i < myServer->segments.seg.size(); i++){
            V_NeuronSWC tmpSeg =  myServer->segments.seg[i];
            for(int j = 0; j < tmpSeg.row.size(); j++){
                if(fabs(tmpSeg.row[j].x - seg.row[0].x) < 1e-5 && fabs(tmpSeg.row[j].y - seg.row[0].y) < 1e-5 &&
                    fabs(tmpSeg.row[j].z - seg.row[0].z) < 1e-5 && index != i){
                    firstSegID = i;
                }
                if(fabs(tmpSeg.row[j].x - seg.row[seg.row.size() - 1].x) < 1e-5 && fabs(tmpSeg.row[j].y - seg.row[seg.row.size() - 1].y) < 1e-5 &&
                    fabs(tmpSeg.row[j].z - seg.row[seg.row.size() - 1].z) < 1e-5 && index != i){
                    secondSegID = i;
                }
            }
        }

        qDebug() << "firstSegID: "<< firstSegID << "secondSegID: " << secondSegID;
        vector<V_NeuronSWC> connectedSegs;
        bool isBegin = true;
        if(firstSegID != -1){
            connectedSegs.push_back(myServer->segments.seg[firstSegID]);
        }
        if(secondSegID != -1){
            connectedSegs.push_back(myServer->segments.seg[secondSegID]);
        }

        if(firstSegID!=-1 && connectedSegs.size()==1){
            isBegin = true;
        }
        if(secondSegID!=-1 && connectedSegs.size()==1){
            isBegin = false;
        }

        QStringList addMsgList;
        addMsgList.append(QString("0 server %1 %2 %3 %4").arg(0).arg(123).arg(123).arg(123));
        addMsgList += V_NeuronSWCToSendMSG(seg);
        addMsgList += "$";
        for(auto connectedSegs_it = connectedSegs.begin(); connectedSegs_it != connectedSegs.end(); connectedSegs_it++){
            addMsgList += V_NeuronSWCToSendMSG(*connectedSegs_it);
            addMsgList += "$";
        }

        QString msg=QString("/drawline_norm:"+addMsgList.join(","));
        msgList.append(msg);
    }

    qDebug() << msgList;
    emit myServer->clientSendMsgs(msgList);
}

vector<V_NeuronSWC> CollDetection::getCrossingCorrectedSegs(){
    vector<V_NeuronSWC> tobeAddedSegs;
    for (auto it = crossingInfoMap.begin(); it != crossingInfoMap.end(); it++) {
        CrossingInfo crossingInfo = it->second;
        if (!crossingInfo.isAbleCorrect) {
            continue;
        }
        pair<FiberCoorData, FiberCoorData> fiberCoorDataPair;
        FiberCoorData fiberCoorData;
        fiberCoorData.segID = crossingInfo.coor2SegIndexPair.first.second;
        fiberCoorData.rangePair = crossingInfo.coor2RowIndexRangePair.first.second;
        fiberCoorData.coorVec = crossingInfo.fiberCoorInfoPair.first.second;
        fiberCoorDataPair.first = fiberCoorData;
        fiberCoorData.segID = crossingInfo.coor2SegIndexPair.second.second;
        fiberCoorData.rangePair = crossingInfo.coor2RowIndexRangePair.second.second;
        fiberCoorData.coorVec = crossingInfo.fiberCoorInfoPair.second.second;
        fiberCoorDataPair.second = fiberCoorData;


        myServer->segments.seg[fiberCoorDataPair.first.segID].to_be_deleted = true;
        myServer->segments.seg[fiberCoorDataPair.second.segID].to_be_deleted = true;
        auto tobeRemovedSegFirst = myServer->segments.seg[fiberCoorDataPair.first.segID];
        auto tobeRemovedSegSecond = myServer->segments.seg[fiberCoorDataPair.second.segID];

        // 截取前面一段和后面一段
        vector<V_NeuronSWC_unit> frontCoorVecFirst(tobeRemovedSegFirst.row.begin(), tobeRemovedSegFirst.row.begin() + fiberCoorDataPair.first.rangePair.first);
        vector<V_NeuronSWC_unit> frontCoorVecSecond(tobeRemovedSegSecond.row.begin(), tobeRemovedSegSecond.row.begin() + fiberCoorDataPair.second.rangePair.first);
        vector<V_NeuronSWC_unit> backCoorVecSecond(tobeRemovedSegSecond.row.begin() + fiberCoorDataPair.second.rangePair.second + 1, tobeRemovedSegSecond.row.end());
        vector<V_NeuronSWC_unit> backCoorVecFirst(tobeRemovedSegFirst.row.begin() + fiberCoorDataPair.first.rangePair.second + 1, tobeRemovedSegFirst.row.end());

        fiberCoorDataPair.first.addedSeg =  tobeRemovedSegFirst;
        fiberCoorDataPair.second.addedSeg =  tobeRemovedSegSecond;
        // 前面一段 + 纠正段 + 后面一段
        fiberCoorDataPair.first.addedSeg.row = frontCoorVecFirst;
        for (auto coorIt = fiberCoorDataPair.first.coorVec.begin(); coorIt != fiberCoorDataPair.first.coorVec.end(); coorIt++){
            V_NeuronSWC_unit unit;
            unit.x = coorIt->x;
            unit.y = coorIt->y;
            unit.z = coorIt->z;
            unit.r = 1;
            fiberCoorDataPair.first.addedSeg.row.push_back(unit);
        }
        fiberCoorDataPair.first.addedSeg.row.insert(fiberCoorDataPair.first.addedSeg.row.end(), backCoorVecSecond.begin(), backCoorVecSecond.end());

        fiberCoorDataPair.second.addedSeg.row = frontCoorVecSecond;
        for (auto coorIt = fiberCoorDataPair.second.coorVec.begin(); coorIt != fiberCoorDataPair.second.coorVec.end(); coorIt++){
            V_NeuronSWC_unit unit;
            unit.x = coorIt->x;
            unit.y = coorIt->y;
            unit.z = coorIt->z;
            unit.r = 1;
            fiberCoorDataPair.second.addedSeg.row.push_back(unit);
        }
        fiberCoorDataPair.second.addedSeg.row.insert(fiberCoorDataPair.second.addedSeg.row.end(), backCoorVecFirst.begin(), backCoorVecFirst.end());

        // 设置n、parent、type
        int n = 1;
        for (auto unitIt = fiberCoorDataPair.first.addedSeg.row.begin(); unitIt != fiberCoorDataPair.first.addedSeg.row.end(); unitIt++){
            unitIt->type = fiberCoorDataPair.first.addedSeg.row[0].type;
            unitIt->n = n++;
            unitIt->parent = n;
        }
        fiberCoorDataPair.first.addedSeg.row[fiberCoorDataPair.first.addedSeg.row.size() - 1].parent = -1;
        n = 1;
        for (auto unitIt = fiberCoorDataPair.second.addedSeg.row.begin(); unitIt != fiberCoorDataPair.second.addedSeg.row.end(); unitIt++){
            unitIt->type = fiberCoorDataPair.second.addedSeg.row[0].type;
            unitIt->n = n++;
            unitIt->parent = n;
        }
        fiberCoorDataPair.second.addedSeg.row[fiberCoorDataPair.second.addedSeg.row.size() - 1].parent = -1;

        tobeAddedSegs.push_back(fiberCoorDataPair.first.addedSeg);
        tobeAddedSegs.push_back(fiberCoorDataPair.second.addedSeg);
    }
    return tobeAddedSegs;
}

void CollDetection::autoCorrectCrossing(vector<V_NeuronSWC> addedSegs){
    // 删线
    QStringList delResult;
    proto::SwcDataV1 delSwcData;
    int count = 0;
    std::vector<V_NeuronSWC>::iterator iter = myServer->segments.seg.begin();
    while (iter != myServer->segments.seg.end())
        if (iter->to_be_deleted){
            delResult += V_NeuronSWCToSendMSG(*iter);
            delResult.push_back("$");

            for(int j = 0; j < iter->row.size(); j++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_x(iter->row[j].x);
                swcNodeInternalData.set_y(iter->row[j].y);
                swcNodeInternalData.set_z(iter->row[j].z);
                swcNodeInternalData.set_radius(iter->row[j].r);
                swcNodeInternalData.set_type(iter->row[j].type);
                swcNodeInternalData.set_mode(iter->row[j].creatmode);

                auto* newData = delSwcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
                newData->mutable_base()->set_uuid(iter->row[j].uuid);
            }
            iter = myServer->segments.seg.erase(iter);
        }
        else
            ++iter;

    if(delSwcData.swcdata_size() > 0){
        proto::DeleteSwcNodeDataResponse response;
        WrappedCall::deleteSwcNodeData(myServer->swcUuid, delSwcData, response, myServer->cachedUserData);
    }

    //最后的1表示多条线
    delResult.insert(0, QString("%1 server correct_crossing %2 %3 %4").arg(0).arg(count).arg(123).arg(1));
    if(count != 0){
        QString msg = QString("/delline_norm:" + delResult.join(","));
        //        qDebug()<<"removeErrorSegs: "<<msg;
        emit myServer->clientSendMsgs({msg});
    }

    QStringList msgList;

    for (auto it = addedSegs.begin(); it != addedSegs.end(); it++) {
        V3DLONG point_size = myServer->segments.nrows();
        proto::SwcDataV1 addSwcData;
        V_NeuronSWC seg = *it;

        if (seg.row.size() == 0){
            continue;
        }

        for (int i=0; i<seg.row.size(); i++) {
            proto::SwcNodeInternalDataV1 swcNodeInternalData;
            swcNodeInternalData.set_n(point_size + i + 1);
            if(i == seg.row.size()-1)
                swcNodeInternalData.set_parent(-1);
            else
                swcNodeInternalData.set_parent(point_size + i + 2);
            swcNodeInternalData.set_x(seg.row[i].x);
            swcNodeInternalData.set_y(seg.row[i].y);
            swcNodeInternalData.set_z(seg.row[i].z);
            swcNodeInternalData.set_radius(seg.row[i].r);
            swcNodeInternalData.set_type(seg.row[i].type);
            swcNodeInternalData.set_mode(seg.row[i].creatmode);

            auto* newData = addSwcData.add_swcdata();
            newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
        }

        proto::CreateSwcNodeDataResponse response;
        if(!WrappedCall::addSwcNodeData(myServer->swcUuid, addSwcData, response, myServer->cachedUserData)){
            QString msg = "/WARN_AddSwcNodeDataError:server";
            emit myServer->clientSendMsgs({msg});
            return;
        }

        auto uuids = response.creatednodesuuid();
        for(int i=0; i<seg.row.size(); i++){
            seg.row[i].uuid = uuids.Get(i);
        }

        myServer->segments.append(seg);

        reverseSeg(seg);
        int firstSegID = -1;
        int secondSegID = -1;
        int index = myServer->segments.seg.size() - 1;
        for(int i = 0; i < myServer->segments.seg.size(); i++){
            V_NeuronSWC tmpSeg =  myServer->segments.seg[i];
            for(int j = 0; j < tmpSeg.row.size(); j++){
                if(fabs(tmpSeg.row[j].x - seg.row[0].x) < 1e-5 && fabs(tmpSeg.row[j].y - seg.row[0].y) < 1e-5 &&
                    fabs(tmpSeg.row[j].z - seg.row[0].z) < 1e-5 && index != i){
                    firstSegID = i;
                }
                if(fabs(tmpSeg.row[j].x - seg.row[seg.row.size() - 1].x) < 1e-5 && fabs(tmpSeg.row[j].y - seg.row[seg.row.size() - 1].y) < 1e-5 &&
                    fabs(tmpSeg.row[j].z - seg.row[seg.row.size() - 1].z) < 1e-5 && index != i){
                    secondSegID = i;
                }
            }
        }

        qDebug() << "firstSegID: "<< firstSegID << "secondSegID: " << secondSegID;
        vector<V_NeuronSWC> connectedSegs;
        bool isBegin = true;
        if(firstSegID != -1){
            connectedSegs.push_back(myServer->segments.seg[firstSegID]);
        }
        if(secondSegID != -1){
            connectedSegs.push_back(myServer->segments.seg[secondSegID]);
        }

        if(firstSegID!=-1 && connectedSegs.size()==1){
            isBegin = true;
        }
        if(secondSegID!=-1 && connectedSegs.size()==1){
            isBegin = false;
        }

        QStringList addMsgList;
        addMsgList.append(QString("0 server %1 %2 %3 %4").arg(0).arg(123).arg(123).arg(123));
        addMsgList += V_NeuronSWCToSendMSG(seg);
        addMsgList += "$";
        for(auto connectedSegs_it = connectedSegs.begin(); connectedSegs_it != connectedSegs.end(); connectedSegs_it++){
            addMsgList += V_NeuronSWCToSendMSG(*connectedSegs_it);
            addMsgList += "$";
        }

        QString msg=QString("/drawline_norm:"+addMsgList.join(","));
        msgList.append(msg);
    }

    qDebug() << msgList;
    emit myServer->clientSendMsgs(msgList);
}

void CollDetection::handleBranchingPoints(vector<NeuronSWC>& brainchingPoints, int& count){
    if(brainchingPoints.size()!=0){
        QJsonObject json;
        QString obj=myServer->getImage();
        json.insert("obj",obj);
        //        json.insert("res", myServer->RES);
        QJsonArray coorList;
        for(int i=0; i<brainchingPoints.size();i++){
            QJsonObject coor;
            coor.insert("x", brainchingPoints[i].x);
            coor.insert("y", brainchingPoints[i].y);
            coor.insert("z", brainchingPoints[i].z);
            coorList.append(coor);
        }
        json.insert("coors",coorList);
        json.insert("swcName",QString::fromStdString(myServer->swcName));
        QJsonDocument document;
        document.setObject(json);
        QString str=QString(document.toJson());
        QByteArray byteArray=str.toUtf8();

        // 创建一个QNetworkRequest对象，设置URL和请求方法
        QNetworkRequest request(QUrl(SuperUserHostAddress+"/detect/branching"));
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        //        request.setRawHeader("Content-Type", "multipart/form-data; boundary=" + multiPart->boundary());

        // 发送HTTP POST请求
        QNetworkReply* reply = accessManager->post(request, byteArray);

        QEventLoop eventLoop;
        connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

        if(reply->error())
        {
            qDebug() << "ERROR!";
            qDebug() << reply->errorString();
        }
        int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug()<<"handleBranchingPoints "<<code;
        QByteArray responseData = reply->readAll();
        vector<NeuronSWC> markPoints;
        if(code==200)
        {
            //解析json
            QJsonParseError json_error;
            QJsonDocument doucment = QJsonDocument::fromJson(responseData, &json_error);
            if (json_error.error == QJsonParseError::NoError) {
                if (doucment.isObject()) {
                    const QJsonObject obj = doucment.object();
                    qDebug() << obj;
                    QString objCode;
                    QString objMsg;
                    if (obj.contains("code")) {
                        QJsonValue value = obj.value("code");
                        if (value.isString()) {
                            objCode= value.toString();
                            qDebug() << "code : " << objCode;
                        }
                    }
                    if (obj.contains("msg")) {
                        QJsonValue value = obj.value("msg");
                        if (value.isString()) {
                            QString objMsg = value.toString();
                            qDebug() << "msg : " << objMsg;
                        }
                    }
                    if (obj.contains("data")&&objCode=="200") {
                        QJsonValue value = obj.value("data");
                        if (value.isArray()) {  // Version 的 value 是数组
                            QJsonArray array = value.toArray();
                            int nSize = array.size();
                            for (int i = 0; i < nSize; ++i) {
                                QJsonValue mapValue = array.at(i);
                                if (mapValue.isObject()) {
                                    QJsonObject info = mapValue.toObject();
                                    float x,y,z;
                                    int y_pred;
                                    if (info.contains("coors")) {
                                        QJsonValue listValue = info.value("coors");
                                        if (listValue.isArray()) {
                                            QJsonArray listArray = listValue.toArray();
                                            QJsonValue xValue = listArray.at(0);
                                            QJsonValue yValue = listArray.at(1);
                                            QJsonValue zValue = listArray.at(2);
                                            x=xValue.toDouble();
                                            y=yValue.toDouble();
                                            z=zValue.toDouble();
                                        }
                                    }
                                    if (info.contains("y_pred")) {
                                        QJsonValue predValue = info.value("y_pred");
                                        y_pred = predValue.toInt();
                                        if(y_pred == 1)
                                            qDebug()<<i<<": "<<y_pred;
                                    }
                                    if(y_pred==1){
                                        NeuronSWC s;
                                        s.x=x;
                                        s.y=y;
                                        s.z=z;
                                        s.type=3;
                                        markPoints.push_back(s);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            std::cerr<<"handle branching error!";
        }

        QString tobeSendMsg=QString("/WARN_BranchingError:server,");
        QStringList result;
        QString comment = "Branching error";

        for(int i=0;i<markPoints.size();i++){
            RGB8 color = getColorFromType(markPoints[i].type);
            QString curMarker=QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(markPoints[i].x).arg(markPoints[i].y).arg(markPoints[i].z);

            QString msg=QString("/WARN_BranchingError:server,"+curMarker);
            bool isSucess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_BranchingError:").size()), comment);
            if(isSucess){
                result.push_back(curMarker);
                count++;
            }
        }

        tobeSendMsg = tobeSendMsg + result.join(",");

        if(count!=0)
            emit myServer->clientSendMsgs({tobeSendMsg});

        qDebug()<<"Server finish /WARN_BranchingError";

        //        if(markPoints.size()!=0)
        //            myServer->imediateSave();

        //清理资源
        reply->deleteLater();
    }
}

void CollDetection::handleCrossing(QJsonArray& infos){
    QString swcFileName=myServer->getAnoName()+".ano.eswc";
    QString fileSavePath=myServer->swcpath.left(myServer->swcpath.size()-QString(".ano.eswc").size())+"_forcrossing.ano.eswc";
    // 创建一个QFile对象，用于读取要上传的文件
    QFile *file = new QFile(fileSavePath);
    file->open(QIODevice::Text|QIODevice::ReadWrite);

    if(!infos.isEmpty()){
        QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart filePart;

        filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("text/plain"));
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"swcFile\"; filename=\""+ swcFileName + "\"")); // file为后端定义的key，filename即为excel文件名
        filePart.setBodyDevice(file);

        multiPart->append(filePart);
        QNetworkRequest fileRequest(QUrl(SuperUserHostAddress+"/detect/file/for-crossing"));
        // 发送HTTP POST请求
        QNetworkReply* fileReply = accessManager->post(fileRequest, multiPart);
        multiPart->setParent(fileReply); // reply对象将负责删除multiPart对象
        QEventLoop tmpeventLoop;
        connect(fileReply, &QNetworkReply::finished, &tmpeventLoop, &QEventLoop::quit);
        tmpeventLoop.exec(QEventLoop::ExcludeUserInputEvents);

        if(fileReply->error())
        {
            qDebug() << "SENDFILEERROR!";
            qDebug() << fileReply->errorString();
        }
        int fileResCode=fileReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug()<<"sendFile"<<fileResCode;

        QJsonObject json;
        QString obj=myServer->getImage();
        json.insert("obj",obj);
//        json.insert("res", myServer->RES);
        json.insert("swcNameWithNoSuffix", myServer->getAnoName());
        json.insert("infos",infos);

        QJsonDocument document;
        document.setObject(json);
        QString str=QString(document.toJson());
        QByteArray byteArray=str.toUtf8();

        // 创建一个QNetworkRequest对象，设置URL和请求方法
        QNetworkRequest request(QUrl(SuperUserHostAddress+"/detect/crossing"));
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        //        request.setRawHeader("Content-Type", "multipart/form-data; boundary=" + multiPart->boundary());

        // 发送HTTP POST请求
        QNetworkReply* reply = accessManager->post(request, byteArray);

        QEventLoop eventLoop;
        connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

        if(reply->error())
        {
            qDebug() << "ERROR!";
            qDebug() << reply->errorString();
        }
        int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug()<<"handleCrossing"<<code;
        QByteArray responseData = reply->readAll();
        vector<NeuronSWC> markPoints;
        if(code==200)
        {
            //解析json
            QJsonParseError json_error;
            QJsonDocument doucment = QJsonDocument::fromJson(responseData, &json_error);
            if (json_error.error == QJsonParseError::NoError) {
                if (doucment.isObject()) {
                    const QJsonObject obj = doucment.object();
                    qDebug() << obj;
                    QString objCode;
                    QString objMsg;
                    if (obj.contains("code")) {
                        QJsonValue value = obj.value("code");
                        if (value.isString()) {
                            objCode= value.toString();
                            qDebug() << "code : " << objCode;
                        }
                    }
                    if (obj.contains("msg")) {
                        QJsonValue value = obj.value("msg");
                        if (value.isString()) {
                            QString objMsg = value.toString();
                            qDebug() << "msg : " << objMsg;
                        }
                    }
                    if (obj.contains("data")&&objCode=="200") {
                        QJsonValue value = obj.value("data");
                        if (value.isObject()) {  // Version 的 value 是数组
                            QJsonObject resData = value.toObject();
                            QJsonArray fiberPredictedResultListJA = resData.value("fiberPredictedResultList").toArray();
                            int nSize = fiberPredictedResultListJA.size();
                            for (int i = 0; i < nSize; ++i) {
                                QJsonValue fiberPredictedResultVal = fiberPredictedResultListJA.at(i);
                                if (fiberPredictedResultVal.isObject()) {
                                    QJsonObject fiberPredictedResultJO = fiberPredictedResultVal.toObject();
                                    pair<pair<QString, vector<XYZ>>, pair<QString, vector<XYZ>>> pairVal;
                                    int y_pred;
                                    float x, y, z;
                                    QString centerCoorStrRep;
                                    if (fiberPredictedResultJO.contains("fiberCoorInfoList")) {
                                        QJsonValue fiberCoorInfoListJV = fiberPredictedResultJO.value("fiberCoorInfoList");
                                        if (fiberCoorInfoListJV.isArray()) {
                                            QJsonArray fiberCoorInfoListJA = fiberCoorInfoListJV.toArray();
                                            vector<XYZ> fiberCoorList;
                                            for (int j = 0; j < fiberCoorInfoListJA.size(); j++){
                                                QJsonObject fiberCoorInfoJO = fiberCoorInfoListJA.at(j).toObject();
                                                QJsonObject coorJO = fiberCoorInfoJO.value("coor").toObject();
                                                QJsonArray fiberCoorListJA = fiberCoorInfoJO.value("fiberCoorList").toArray();
                                                XYZ centerCoor = XYZ(coorJO.value("x").toDouble(),
                                                                     coorJO.value("y").toDouble(),
                                                                     coorJO.value("z").toDouble());
                                                QString centerCoorStr = QString::number(centerCoor.x) + "_" + QString::number(centerCoor.y) + "_" + QString::number(centerCoor.z);
                                                if (j == 0) {
                                                    x = centerCoor.x;
                                                    y = centerCoor.y;
                                                    z = centerCoor.z;
                                                    centerCoorStrRep = centerCoorStr;
                                                }

                                                for (int p = 0; p < fiberCoorListJA.size(); p++){
                                                    QJsonObject fiberCoorJO = fiberCoorListJA.at(p).toObject();
                                                    XYZ fiberCoor = XYZ(fiberCoorJO.value("x").toDouble(),
                                                                        fiberCoorJO.value("y").toDouble(),
                                                                        fiberCoorJO.value("z").toDouble());
                                                    fiberCoorList.push_back(fiberCoor);
                                                }
                                                if (j == 0) {
                                                    pairVal.first = make_pair(centerCoorStr, fiberCoorList);
                                                }
                                                if (j == 1) {
                                                    pairVal.second = make_pair(centerCoorStr, fiberCoorList);
                                                }
                                            }
                                        }
                                    }
                                    if (fiberPredictedResultJO.contains("y_pred")) {
                                        QJsonValue predValue = fiberPredictedResultJO.value("y_pred");
                                        y_pred = predValue.toInt();
                                    }
                                    if (y_pred == 2) {
                                        crossingInfoMap[centerCoorStrRep].fiberCoorInfoPair = pairVal;
                                        if (y_pred != 2) {
                                            crossingInfoMap[centerCoorStrRep].isAbleCorrect = false;
                                        }
                                    }
                                    if (y_pred == 0) {
                                        NeuronSWC s;
                                        s.x = x;
                                        s.y = y;
                                        s.z = z;
                                        s.type = 18;
                                        markPoints.push_back(s);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            std::cerr<<"handle crossing error!";
        }

        if (isAutoCorrect) {
            fliterCrossing();
            auto tobeAddedSegs = getCrossingCorrectedSegs();
            myServer->mutex.lock();
            autoCorrectCrossing(tobeAddedSegs);
            myServer->mutex.unlock();

            //清理资源
            crossingInfoMap.clear();
            reply->deleteLater();
            QFile::remove(fileSavePath);
            file->close();
            file->deleteLater();
            return;
        }

        QString tobeSendMsg=QString("/WARN_CrossingError:server,");
        QStringList result;
        QString comment = "Crossing error";
        int count = 0;

        for(int i=0;i<markPoints.size();i++){
            RGB8 color = getColorFromType(markPoints[i].type);
            QString curMarker=QString("%1 %2 %3 %4 %5 %6").arg(color.r).arg(color.g).arg(color.b).arg(markPoints[i].x).arg(markPoints[i].y).arg(markPoints[i].z);

            QString msg=QString("/WARN_CrossingError:server,"+curMarker);
            bool isSucess=myServer->addmarkers(msg.trimmed().right(msg.size()-QString("/WARN_CrossingError:").size()), comment);
            if(isSucess){
                result.push_back(curMarker);
                count++;
            }
        }

        tobeSendMsg = tobeSendMsg + result.join(",");

        if(count!=0)
            emit myServer->clientSendMsgs({tobeSendMsg});

        qDebug()<<"Server finish /WARN_CrossingError";

//        if(markPoints.size()!=0)
//            myServer->imediateSave();

        //清理资源
        reply->deleteLater();
    }

    //清理资源
    QFile::remove(fileSavePath);
    file->close();
    file->deleteLater();
}

void CollDetection::sortSWC(QString fileOpenName, QString fileSaveName, double thres, V3DLONG rootid){
    QList<NeuronSWC> neuron, result;
    if (fileOpenName.endsWith(".swc") || fileOpenName.endsWith(".SWC") || fileOpenName.endsWith(".eswc") || fileOpenName.endsWith(".ESWC"))
        neuron = readSWC_file(fileOpenName).listNeuron;
    if (!SortSWC(neuron, result , rootid, thres))
    {
        cout<<"Error in sorting swc"<<endl;
    }
    if (!export_list2file(result, fileSaveName, fileOpenName))
    {
        cout<<"Error in writing swc to file"<<endl;
    }
}

void CollDetection::setSWCRadius(QString filePath, int r){
    if (filePath.endsWith(".swc") || filePath.endsWith(".SWC") || filePath.endsWith(".eswc") || filePath.endsWith(".ESWC"))
    {
        QFile qf(filePath);
        QString arryRead;
        if(!qf.open(QIODevice::ReadOnly|QIODevice::Text)){
            return;
        }
        arryRead=qf.readAll();

        qf.close();

        QStringList arryListWrite= arryRead.split("\n");
//        for(int i=0;i<arryListWrite.size();i++){
//            qDebug()<<arryListWrite.at(i);
//        }

        // QIODevice::Text:以文本方式打开文件，读取时“\n”被自动翻译为换行符，写入时字符串结束符会自动翻译为系统平台的编码，如 Windows 平台下是“\r\n”
        if (!qf.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug()<<"swcfile cannot be opened!";
            return;
        }
        QTextStream streamWrite(&qf);
        for(int i=0;i<arryListWrite.size()-1;i++){      //这里到arryListWrite.size()-1是因为arryListWrite数组按照\n分 段时，最后一行尾部有个\n，所以数组最后一个值为空，需要将它去掉
            if(arryListWrite.at(i).contains("#")){
                streamWrite<<arryListWrite.at(i)<<"\n";
            }else{
                QString contentWrite= arryListWrite.at(i);
                QStringList swcInfo=contentWrite.split(' ',Qt::SkipEmptyParts);
                swcInfo[5]=QString::number(r);
                contentWrite=swcInfo.join(' ');
                streamWrite<<contentWrite<<"\n";
            }
        }
        qf.close();
    }
}

void CollDetection::getImageRES(){
    QJsonObject json;
    QString obj=myServer->getImage();
    QJsonObject userMap;
    userMap.insert("name", "zackzhy");
    userMap.insert("passwd", "123456");
    QJsonObject imageMap;
    imageMap.insert("name", obj);
    imageMap.insert("detail", "");
    json.insert("user", userMap);
    json.insert("Image", imageMap);

    QJsonDocument document;
    document.setObject(json);
    QString str=QString(document.toJson());
    QByteArray byteArray=str.toUtf8();

    // 创建一个QNetworkRequest对象，设置URL和请求方法
    QNetworkRequest request(QUrl(BrainTellHostAddress+"/image/getimagelist"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");

    // 发送HTTP POST请求
    QNetworkReply* reply = accessManager->post(request, byteArray);

    QEventLoop eventLoop;
    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

    if(reply->error())
    {
        qDebug() << "ERROR!";
        qDebug() << reply->errorString();
    }
    int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug()<<"getImageRes "<<code;
    QByteArray responseData = reply->readAll();

    QString maxResString;
    QString subMaxResString;
    if(code==200){
        //解析json
        QJsonParseError json_error;
        QJsonDocument doucment = QJsonDocument::fromJson(responseData, &json_error);
        if (json_error.error == QJsonParseError::NoError) {
            if (doucment.isArray()) {
                const QJsonArray array = doucment.array();
//                qDebug() << array;
                int nSize = array.size();
                for (int i = 0; i < nSize; ++i) {
                    QJsonValue mapValue = array.at(i);
                    if (mapValue.isObject()) {
                        QJsonObject info = mapValue.toObject();
                        QString name="";
                        QString details;
                        if(info.contains("name")){
                            QJsonValue nameVal=info.value("name");
                            if(nameVal.isString()){
                                name=nameVal.toString();
                            }
                        }

                        if(name!=obj)
                            continue;
                        if(info.contains("detail")){
                            QJsonValue detailsVal=info.value("detail");
                            if(detailsVal.isString()){
                                details=detailsVal.toString();
                                QJsonDocument detailsDoc=QJsonDocument::fromJson(details.toUtf8());
                                if(detailsDoc.isArray()){
                                    QJsonArray detailsArray=detailsDoc.array();
                                    maxResString=detailsArray.at(0).toString();
                                    subMaxResString=detailsArray.at(1).toString();
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    QRegularExpression regex("^RES\\((\\d+)x(\\d+)x(\\d+)\\)");
    QRegularExpressionMatchIterator matches = regex.globalMatch(maxResString);

    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString matchedText = match.captured(0);
        QString matchedY = match.captured(1);
        QString matchedX = match.captured(2);
        QString matchedZ = match.captured(3);
        maxRes.x=matchedX.toFloat();
        maxRes.y=matchedY.toFloat();
        maxRes.z=matchedZ.toFloat();
    }

    matches=regex.globalMatch(subMaxResString);
    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString matchedText = match.captured(0);
        QString matchedY = match.captured(1);
        QString matchedX = match.captured(2);
        QString matchedZ = match.captured(3);
        subMaxRes.x=matchedX.toFloat();
        subMaxRes.y=matchedY.toFloat();
        subMaxRes.z=matchedZ.toFloat();
    }

    //清理资源
    reply->deleteLater();
}

void CollDetection::getImageMaxRES(){
    // 定义正则表达式
    QRegularExpression regex("RES\\((\\d+)x(\\d+)x(\\d+)\\)");

    // 进行匹配
    QRegularExpressionMatch match = regex.match(myServer->RES);

    // 检查匹配是否成功
    if (match.hasMatch()) {
        // 提取宽度、高度和深度
        QString yStr = match.captured(1);
        QString xStr = match.captured(2);
        QString zStr = match.captured(3);

        maxRes.x=xStr.toInt();
        maxRes.y=yStr.toInt();
        maxRes.z=zStr.toInt();
    } else {
        qDebug() << "No match found";
    }
}

void CollDetection::getApoForCrop(QString fileSaveName, vector<NeuronSWC> tipPoints){
    qDebug()<<maxRes.x<<" "<<maxRes.y<<" "<<maxRes.z;
    qDebug()<<subMaxRes.x<<" "<<subMaxRes.y<<" "<<subMaxRes.z;
    QList <CellAPO> markers;
    for(int i=0;i<tipPoints.size();i++){
        tipPoints[i].x/=(maxRes.x/subMaxRes.x);
        tipPoints[i].y/=(maxRes.y/subMaxRes.y);
        tipPoints[i].z/=(maxRes.z/subMaxRes.z);
        CellAPO marker;
        marker.color.r=neuron_type_color[2][0];
        marker.color.g=neuron_type_color[2][1];
        marker.color.b=neuron_type_color[2][2];
        marker.x=tipPoints[i].x;
        marker.y=tipPoints[i].y;
        marker.z=tipPoints[i].z;
        markers.append(marker);
    }

    writeAPO_file(fileSaveName, markers);
}

void CollDetection::removeErrorSegs(bool flag){
    for(size_t i=0; i<myServer->segments.seg.size(); ++i){
        V_NeuronSWC seg = myServer->segments.seg[i];
        if(seg.row.size()==1){
            myServer->segments.seg[i].to_be_deleted = true;
            auto seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), myServer->segments.seg[i]);
            if(seg_it != myServer->last1MinSegments.seg.end()){
                seg_it->to_be_deleted = true;
            }

            seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), myServer->segments.seg[i]);
            if(seg_it != myServer->last3MinSegments.seg.end()){
                seg_it->to_be_deleted = true;
            }
            continue;
        }

        set<string> coors;
        for(size_t j=0; j<seg.row.size(); j++){
            float xLabel = seg.row[j].x;
            float yLabel = seg.row[j].y;
            float zLabel = seg.row[j].z;
            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
            string gridKey = gridKeyQ.toStdString();
            coors.insert(gridKey);
        }

        if(coors.size() < seg.row.size())
        {
            myServer->segments.seg[i].to_be_deleted = true;
            auto seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), myServer->segments.seg[i]);
            if(seg_it != myServer->last1MinSegments.seg.end()){
                seg_it->to_be_deleted = true;
            }

            seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), myServer->segments.seg[i]);
            if(seg_it != myServer->last3MinSegments.seg.end()){
                seg_it->to_be_deleted = true;
            }
            continue;
        }
    }

    std::vector<V_NeuronSWC>::iterator iter = myServer->last1MinSegments.seg.begin();
    while (iter != myServer->last1MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last1MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->last3MinSegments.seg.begin();
    while (iter != myServer->last3MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last3MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->segments.seg.begin();
    QStringList result;
    int count = 0;
    bool isSend = false;

    proto::SwcDataV1 swcData;
    while (iter != myServer->segments.seg.end())
        if (iter->to_be_deleted){
            myServer->removedErrSegNum++;
            isSend = true;
            count++;
            result+=V_NeuronSWCToSendMSG(*iter);
            result.push_back("$");

            for(int j=0; j<iter->row.size(); j++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_x(iter->row[j].x);
                swcNodeInternalData.set_y(iter->row[j].y);
                swcNodeInternalData.set_z(iter->row[j].z);
                swcNodeInternalData.set_radius(iter->row[j].r);
                swcNodeInternalData.set_type(iter->row[j].type);
                swcNodeInternalData.set_mode(iter->row[j].creatmode);

                auto* newData = swcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
                newData->mutable_base()->set_uuid(iter->row[j].uuid);
            }

            iter->printInfo();

            iter = myServer->segments.seg.erase(iter);
        }
        else
            ++iter;

    if(swcData.swcdata_size() > 0){
        proto::DeleteSwcNodeDataResponse response;
        WrappedCall::deleteSwcNodeData(myServer->swcUuid, swcData, response, myServer->cachedUserData);
    }

    result.insert(0,QString("%1 server error %2 %3 %4").arg(0).arg(count).arg(123).arg(1));

    QString msg=QString("/delline_norm:"+result.join(","));
    if(isSend){
        qDebug()<<"removeErrorSegs: "<<msg;
        emit myServer->clientSendMsgs({msg});
    }

    if(flag)
        emit removeErrorSegsDone();
}

void CollDetection::tuneErrorSegs(bool flag){
    vector<pair<V_NeuronSWC, V_NeuronSWC>> errorSegPairVec;
    proto::SwcDataV1 delSwcData;
    for(size_t i=0; i<myServer->segments.seg.size(); ++i){
        V_NeuronSWC seg = myServer->segments.seg[i];
        if(seg.row.size()==1){
            myServer->segments.seg[i].to_be_deleted = true;
            auto seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), myServer->segments.seg[i]);
            if(seg_it != myServer->last1MinSegments.seg.end()){
                seg_it->to_be_deleted = true;
            }

            seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), myServer->segments.seg[i]);
            if(seg_it != myServer->last3MinSegments.seg.end()){
                seg_it->to_be_deleted = true;
            }
            continue;
        }

//        set<string> coors;
//        for(size_t j=0; j<seg.row.size(); j++){
//            float xLabel = seg.row[j].x;
//            float yLabel = seg.row[j].y;
//            float zLabel = seg.row[j].z;
//            QString gridKeyQ = QString::number(xLabel) + "_" + QString::number(yLabel) + "_" + QString::number(zLabel);
//            string gridKey = gridKeyQ.toStdString();
//            coors.insert(gridKey);
//        }

//        if(coors.size() < seg.row.size())
//        {
//            pair<V_NeuronSWC, V_NeuronSWC> segPair;
//            segPair.first = seg;

//            for (auto it = seg.row.begin(); it!=seg.row.end() - 1 && it!=seg.row.end();)
//            {
//                V_NeuronSWC_unit v1 = *it;
//                V_NeuronSWC_unit v2 = *(it+1);
//                if((fabs(v1.x - v2.x) > 1e-2) || (fabs(v1.y - v2.y) > 1e-2) || (fabs(v1.z - v2.z) > 1e-2)){
//                    it++;
//                }
//                else{
//                    it = seg.row.erase(it);
//                }
//            }

//            int count = 1;
//            for (V3DLONG p=0;p<seg.row.size();p++)
//            {
//                V_NeuronSWC_unit& v = seg.row.at(p);
//                v.n = count++;
//                v.parent = count;
//            }
//            seg.row[seg.row.size() - 1].parent = -1;
//            segPair.second = seg;

//            if(segPair.first.row.size() == segPair.second.row.size()){
//                continue;
//            }

////            qDebug() << "1111111111111111111111";
//            myServer->segments.seg[i].to_be_deleted = true;
////            myServer->removedErrSegNum++;
//            errorSegPairVec.push_back(segPair);
//        }
    }

    QStringList result;
    int count = 0;
    for(auto it = errorSegPairVec.begin(); it != errorSegPairVec.end(); it++){
        auto seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), it->first);
        if(seg_it != myServer->last1MinSegments.seg.end()){
            seg_it->to_be_deleted = true;
        }

        seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), it->first);
        if(seg_it != myServer->last3MinSegments.seg.end()){
            seg_it->to_be_deleted = true;
        }

//        result+=V_NeuronSWCToSendMSG(it->first);
//        result.push_back("$");
//        count++;
    }

    //以上完成异常seg的标记

    //删除segments中的线、数据库中的线、客户端的线
    std::vector<V_NeuronSWC>::iterator iter = myServer->segments.seg.begin();
    while (iter != myServer->segments.seg.end())
        if (iter->to_be_deleted){
            myServer->removedErrSegNum++;
            count++;
            result+=V_NeuronSWCToSendMSG(*iter);
            result.push_back("$");

            for(int j=0; j<iter->row.size(); j++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_x(iter->row[j].x);
                swcNodeInternalData.set_y(iter->row[j].y);
                swcNodeInternalData.set_z(iter->row[j].z);
                swcNodeInternalData.set_radius(iter->row[j].r);
                swcNodeInternalData.set_type(iter->row[j].type);
                swcNodeInternalData.set_mode(iter->row[j].creatmode);

                auto* newData = delSwcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
                newData->mutable_base()->set_uuid(iter->row[j].uuid);
            }
            iter = myServer->segments.seg.erase(iter);
        }
        else
            ++iter;

    if(delSwcData.swcdata_size() > 0){
        proto::DeleteSwcNodeDataResponse response;
        WrappedCall::deleteSwcNodeData(myServer->swcUuid, delSwcData, response, myServer->cachedUserData);
    }

    //最后的1表示多条线
    result.insert(0, QString("%1 server error %2 %3 %4").arg(0).arg(count).arg(123).arg(1));
    if(count != 0){
        QString msg=QString("/delline_norm:"+result.join(","));
//        qDebug()<<"removeErrorSegs: "<<msg;
        emit myServer->clientSendMsgs({msg});
    }

    //删除两个增量数据结构中的线
    iter = myServer->last1MinSegments.seg.begin();
    while (iter != myServer->last1MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last1MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->last3MinSegments.seg.begin();
    while (iter != myServer->last3MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last3MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    //加线
    for(auto it = errorSegPairVec.begin(); it != errorSegPairVec.end(); it++){
        if(it->second.row.size() != 1){
            V3DLONG point_size = myServer->segments.nrows();
            proto::SwcDataV1 addSwcData;

            for(int i=0; i<it->second.row.size(); i++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_n(point_size + i + 1);
                if(i == it->second.row.size()-1)
                    swcNodeInternalData.set_parent(-1);
                else
                    swcNodeInternalData.set_parent(point_size + i + 2);
                swcNodeInternalData.set_x(it->second.row[i].x);
                swcNodeInternalData.set_y(it->second.row[i].y);
                swcNodeInternalData.set_z(it->second.row[i].z);
                swcNodeInternalData.set_radius(it->second.row[i].r);
                swcNodeInternalData.set_type(it->second.row[i].type);
                swcNodeInternalData.set_mode(it->second.row[i].creatmode);

                auto* newData = addSwcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
            }

            proto::CreateSwcNodeDataResponse response;
            if(!WrappedCall::addSwcNodeData(myServer->swcUuid, addSwcData, response, myServer->cachedUserData)){
                QString msg = "/WARN_AddSwcNodeDataError:server";
                emit myServer->clientSendMsgs({msg});
                return;
            }

            auto uuids = response.creatednodesuuid();
            for(int i=0; i<it->second.row.size(); i++){
                it->second.row[i].uuid = uuids.Get(i);
            }

            myServer->segments.append(it->second);
            myServer->last1MinSegments.append(it->second);
            myServer->last3MinSegments.append(it->second);

            reverseSeg(it->second);
            int firstSegID = -1;
            int secondSegID = -1;
            int index = myServer->segments.seg.size() - 1;
            for(int i = 0; i < myServer->segments.seg.size(); i++){
                V_NeuronSWC seg =  myServer->segments.seg[i];
                for(int j = 0; j < seg.row.size(); j++){
                    if(fabs(seg.row[j].x - it->second.row[0].x) < 1e-5 && fabs(seg.row[j].y - it->second.row[0].y) < 1e-5 &&
                        fabs(seg.row[j].z - it->second.row[0].z) < 1e-5 && index != i){
                        firstSegID = i;
                    }
                    if(fabs(seg.row[j].x - it->second.row[it->second.row.size() - 1].x) < 1e-5 && fabs(seg.row[j].y - it->second.row[it->second.row.size() - 1].y) < 1e-5 &&
                        fabs(seg.row[j].z - it->second.row[it->second.row.size() - 1].z) < 1e-5 && index != i){
                        secondSegID = i;
                    }
                }
            }

            qDebug() << "firstSegID: "<< firstSegID << "secondSegID: " << secondSegID;
            vector<V_NeuronSWC> connectedSegs;
            bool isBegin = true;
            if(firstSegID != -1){
                connectedSegs.push_back(myServer->segments.seg[firstSegID]);
            }
            if(secondSegID != -1){
                connectedSegs.push_back(myServer->segments.seg[secondSegID]);
            }

            if(firstSegID!=-1 && connectedSegs.size()==1){
                isBegin = true;
            }
            if(secondSegID!=-1 && connectedSegs.size()==1){
                isBegin = false;
            }

            QStringList addMsgList;
            addMsgList.append(QString("0 server %1 %2 %3 %4").arg(0).arg(123).arg(123).arg(123));
            addMsgList += V_NeuronSWCToSendMSG(it->second);
            addMsgList += "$";
            for(auto connectedSegs_it = connectedSegs.begin(); connectedSegs_it != connectedSegs.end(); connectedSegs_it++){
                addMsgList += V_NeuronSWCToSendMSG(*connectedSegs_it);
                addMsgList += "$";
            }

            QString msg=QString("/drawline_norm:"+addMsgList.join(","));
//            qDebug()<<"drawline for correcting error seg: "<<msg;
            emit myServer->clientSendMsgs({msg});
        }
    }

    if(flag)
        emit tuneErrorSegsDone();
}

void CollDetection::removeShortSegs(V_NeuronSWC_list inputSegList, double dist_thre){
    map<string, set<size_t>> wholeGrid2SegIDMap = getWholeGrid2SegIDMap(inputSegList);
    for(auto it=wholeGrid2SegIDMap.begin(); it!=wholeGrid2SegIDMap.end(); it++)
    {
        if(myServer->isSomaExists){
            NeuronSWC s;
            stringToXYZ(it->first, s.x, s.y, s.z);
            if(distance(s.x, myServer->somaCoordinate.x, s.y, myServer->somaCoordinate.y,
                         s.z, myServer->somaCoordinate.z) < dist_thre)
                continue;
        }
        set<size_t> segIds = it->second;
        if(segIds.size() >= 4){
            for(auto segIt=segIds.begin(); segIt!=segIds.end(); segIt++){
                if(inputSegList.seg[*segIt].row.size() <= 3){
                    float xLabel1 = inputSegList.seg[*segIt].row[0].x;
                    float yLabel1 = inputSegList.seg[*segIt].row[0].y;
                    float zLabel1 = inputSegList.seg[*segIt].row[0].z;
                    float xLabel2= inputSegList.seg[*segIt].row[inputSegList.seg[*segIt].row.size()-1].x;
                    float yLabel2= inputSegList.seg[*segIt].row[inputSegList.seg[*segIt].row.size()-1].y;
                    float zLabel2= inputSegList.seg[*segIt].row[inputSegList.seg[*segIt].row.size()-1].z;
                    QString gridKeyQ1 = QString::number(xLabel1) + "_" + QString::number(yLabel1) + "_" + QString::number(zLabel1);
                    string gridKey1 = gridKeyQ1.toStdString();
                    QString gridKeyQ2 = QString::number(xLabel2) + "_" + QString::number(yLabel2) + "_" + QString::number(zLabel2);
                    string gridKey2 = gridKeyQ2.toStdString();
                    int size1 = wholeGrid2SegIDMap[gridKey1].size();
                    int size2 = wholeGrid2SegIDMap[gridKey2].size();
                    double length = getSegLength(inputSegList.seg[*segIt]);
                    if((gridKeyQ1==gridKeyQ2)||((size1<=1 || size2<=1) && length < 4))
                    {
                        auto seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), inputSegList.seg[*segIt]);
                        if(seg_it != myServer->last1MinSegments.seg.end()){
                            seg_it->to_be_deleted = true;
                        }

                        seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), inputSegList.seg[*segIt]);
                        if(seg_it != myServer->last3MinSegments.seg.end()){
                            seg_it->to_be_deleted = true;
                        }

                        seg_it = findseg(myServer->segments.seg.begin(), myServer->segments.seg.end(), inputSegList.seg[*segIt]);
                        if(seg_it != myServer->segments.seg.end()){
                            seg_it->to_be_deleted = true;
                        }

                        if(myServer->segmentsForOthersDetect.seg.size()!=0){
                            seg_it = findseg(myServer->segmentsForOthersDetect.seg.begin(), myServer->segmentsForOthersDetect.seg.end(), inputSegList.seg[*segIt]);
                            if(seg_it != myServer->segmentsForOthersDetect.seg.end()){
                                seg_it->to_be_deleted = true;
                            }
                        }
                    }
                }
            }
        }
    }
    std::vector<V_NeuronSWC>::iterator iter = myServer->last1MinSegments.seg.begin();
    while (iter != myServer->last1MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last1MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->last3MinSegments.seg.begin();
    while (iter != myServer->last3MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last3MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->segmentsForOthersDetect.seg.begin();
    while (iter != myServer->segmentsForOthersDetect.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->segmentsForOthersDetect.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->segments.seg.begin();
    QStringList result;
    bool isSend = false;
    result.push_back(QString("%1 server %2 %3 %4 %5").arg(0).arg(123).arg(123).arg(123).arg(1));
    proto::SwcDataV1 swcData;
    while (iter != myServer->segments.seg.end())
        if (iter->to_be_deleted){
            isSend = true;
            result+=V_NeuronSWCToSendMSG(*iter);
            result.push_back("$");

            for(int j=0; j<iter->row.size(); j++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_x(iter->row[j].x);
                swcNodeInternalData.set_y(iter->row[j].y);
                swcNodeInternalData.set_z(iter->row[j].z);
                swcNodeInternalData.set_radius(iter->row[j].r);
                swcNodeInternalData.set_type(iter->row[j].type);
                swcNodeInternalData.set_mode(iter->row[j].creatmode);

                auto* newData = swcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
                newData->mutable_base()->set_uuid(iter->row[j].uuid);
            }

            iter = myServer->segments.seg.erase(iter);
        }
        else
            ++iter;

    if(swcData.swcdata_size() > 0){
        proto::DeleteSwcNodeDataResponse response;
        WrappedCall::deleteSwcNodeData(myServer->swcUuid, swcData, response, myServer->cachedUserData);
    }

    QString msg=QString("/delline_norm:"+result.join(","));
    if(isSend){
        qDebug()<<"removeShortSegs: "<<msg;
        emit myServer->clientSendMsgs({msg});
    }
}

void CollDetection::removeOverlapSegs(V_NeuronSWC_list inputSegList){
    set<size_t> overlapSegIds;

    //检测overlap线段
    for(size_t i=0; i<inputSegList.seg.size(); ++i){
        float xLabel1 = inputSegList.seg[i].row[0].x;
        float yLabel1 = inputSegList.seg[i].row[0].y;
        float zLabel1 = inputSegList.seg[i].row[0].z;
        float xLabel2 = inputSegList.seg[i].row[inputSegList.seg[i].row.size()-1].x;
        float yLabel2 = inputSegList.seg[i].row[inputSegList.seg[i].row.size()-1].y;
        float zLabel2 = inputSegList.seg[i].row[inputSegList.seg[i].row.size()-1].z;

        if(myServer->isSomaExists){
            if(distance(xLabel1, myServer->somaCoordinate.x, yLabel1, myServer->somaCoordinate.y, zLabel1, myServer->somaCoordinate.z) < 1)
                continue;
            if(distance(xLabel2, myServer->somaCoordinate.x, yLabel2, myServer->somaCoordinate.y, zLabel2, myServer->somaCoordinate.z) < 1)
                continue;
        }

        for(size_t j=i+1; j<inputSegList.seg.size(); j++){
            float xLabel3 = inputSegList.seg[j].row[0].x;
            float yLabel3 = inputSegList.seg[j].row[0].y;
            float zLabel3 = inputSegList.seg[j].row[0].z;
            float xLabel4 = inputSegList.seg[j].row[inputSegList.seg[j].row.size()-1].x;
            float yLabel4 = inputSegList.seg[j].row[inputSegList.seg[j].row.size()-1].y;
            float zLabel4 = inputSegList.seg[j].row[inputSegList.seg[j].row.size()-1].z;

            if(myServer->isSomaExists){
                if(distance(xLabel3, myServer->somaCoordinate.x, yLabel3, myServer->somaCoordinate.y, zLabel3, myServer->somaCoordinate.z) < 1)
                    continue;
                if(distance(xLabel4, myServer->somaCoordinate.x, yLabel4, myServer->somaCoordinate.y, zLabel4, myServer->somaCoordinate.z) < 1)
                    continue;
            }
            int result = isOverlapOfTwoSegs(inputSegList.seg[i], inputSegList.seg[j]);
            if(result == 1)
                overlapSegIds.insert(i);
            if(result == 2)
                overlapSegIds.insert(j);
        }
    }

    for(auto it=overlapSegIds.begin(); it!=overlapSegIds.end(); it++){
        auto seg_it = findseg(myServer->segments.seg.begin(), myServer->segments.seg.end(), inputSegList.seg[*it]);
        if(seg_it != myServer->segments.seg.end()){
            seg_it->to_be_deleted = true;
        }

        seg_it = findseg(myServer->last1MinSegments.seg.begin(), myServer->last1MinSegments.seg.end(), inputSegList.seg[*it]);
        if(seg_it != myServer->last1MinSegments.seg.end()){
            seg_it->to_be_deleted = true;
        }

        seg_it = findseg(myServer->last3MinSegments.seg.begin(), myServer->last3MinSegments.seg.end(), inputSegList.seg[*it]);
        if(seg_it != myServer->last3MinSegments.seg.end()){
            seg_it->to_be_deleted = true;
        }

        if(myServer->segmentsForOthersDetect.seg.size()!=0){
            seg_it = findseg(myServer->segmentsForOthersDetect.seg.begin(), myServer->segmentsForOthersDetect.seg.end(), inputSegList.seg[*it]);
            if(seg_it != myServer->segmentsForOthersDetect.seg.end()){
                seg_it->to_be_deleted = true;
            }
        }
    }

    std::vector<V_NeuronSWC>::iterator iter = myServer->segments.seg.begin();
    QStringList result;
    int count = 0;
    bool isSend = false;

    proto::SwcDataV1 swcData;
    while (iter != myServer->segments.seg.end())
        if (iter->to_be_deleted){
            isSend = true;
            myServer->removedOverlapSegNum++;
            count++;
            result+=V_NeuronSWCToSendMSG(*iter);
            result.push_back("$");

            for(int j=0; j<iter->row.size(); j++){
                proto::SwcNodeInternalDataV1 swcNodeInternalData;
                swcNodeInternalData.set_x(iter->row[j].x);
                swcNodeInternalData.set_y(iter->row[j].y);
                swcNodeInternalData.set_z(iter->row[j].z);
                swcNodeInternalData.set_radius(iter->row[j].r);
                swcNodeInternalData.set_type(iter->row[j].type);
                swcNodeInternalData.set_mode(iter->row[j].creatmode);

                auto* newData = swcData.add_swcdata();
                newData->mutable_swcnodeinternaldata()->CopyFrom(swcNodeInternalData);
                newData->mutable_base()->set_uuid(iter->row[j].uuid);
            }
            iter = myServer->segments.seg.erase(iter);
        }
        else
            ++iter;

    if(swcData.swcdata_size() > 0){
        proto::DeleteSwcNodeDataResponse response;
        WrappedCall::deleteSwcNodeData(myServer->swcUuid, swcData, response, myServer->cachedUserData);
    }

    result.insert(0,QString("%1 server overlap %2 %3 %4").arg(0).arg(count).arg(123).arg(1));

    QString msg=QString("/delline_norm:"+result.join(","));
    if(isSend){
        qDebug()<<"removeOverlapSegs: "<<msg;
        emit myServer->clientSendMsgs({msg});
    }

    iter = myServer->last1MinSegments.seg.begin();
    while (iter != myServer->last1MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last1MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->last3MinSegments.seg.begin();
    while (iter != myServer->last3MinSegments.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->last3MinSegments.seg.erase(iter);
        }
        else
            ++iter;

    iter = myServer->segmentsForOthersDetect.seg.begin();
    while (iter != myServer->segmentsForOthersDetect.seg.end())
        if (iter->to_be_deleted){
            iter = myServer->segmentsForOthersDetect.seg.erase(iter);
        }
        else
            ++iter;

}

