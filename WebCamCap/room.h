/*
 *
 * Copyright (C) 2014  Miroslav Krajicek, Faculty of Informatics Masaryk University (https://github.com/kaajo).
 * All Rights Reserved.
 *
 * This file is part of WebCamCap.
 *
 * WebCamCap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU LGPL version 3 as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WebCamCap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU LGPL version 3
 * along with WebCamCap. If not, see <http://www.gnu.org/licenses/lgpl-3.0.txt>.
 *
 */

#ifndef Room_H
#define Room_H


#include "animation.h"
#include "localserver.h"
#include "pointchecker.h"
#include "capturethread.h"


#include <QTime>


class Edge
{
public:
    Edge(int a, int b, float Eps): m_index1(a), m_index2(b), m_maxError(Eps) {}

    size_t m_index1, m_index2;
    QVector<Line> a ,b;
    float m_maxError;
    QVector <glm::vec3> points;
};

class Room : public QObject
{
    Q_OBJECT

    //basic parameters for project
    QString m_name;
    glm::vec3 m_roomDimensions; //centimeters
    double m_maxError;
    bool m_saved;
    OpenGLWindow *m_openGLWindow = nullptr;

    std::vector <Edge> m_cameraTopology;
    std::vector <CaptureCamera*> m_cameras;

    size_t m_activeCamerasCount;
    size_t m_lastActiveCamIndex;

    bool m_record = false;
    bool m_captureAnimation = false;
    Animation* m_actualAnimation = nullptr;
    std::vector<Animation*> m_animations;

    //pipe
    LocalServer *m_server = nullptr;

    //parallel
    QWaitCondition m_linesWaitCondition;

    //cams
    std::vector <bool> m_haveResults;
    QVector<QVector<Line>> m_resultLines;

    std::vector <worker*> workers;
    std::vector <QThread*> workerthreads;

    //intersections
    QTime m_frameTimer;

    std::vector<glm::vec3> m_points3D;
    std::vector<glm::vec2> m_points2D; //2Drecording

    PointChecker m_pointChecker;
    std::vector<Point> m_labeledPoints;

public:
    Room(OpenGLWindow *openGLWindow = nullptr, glm::vec3 dimensions = glm::vec3(0.0f,0.0f, 0.0f), float eps = 0.5, QString m_name = "Default Project");
    //Room(std::string file);
    ~Room();

    void fromVariantMap(OpenGLWindow *opengl ,QVariantMap &varMap);
    QVariantMap toVariantMap();

    void addCamera(CaptureCamera *cam);
    void removeCamera(size_t index);
    //void Save(std::ofstream &file);

    void makeTopology();
    void resolveTopologyDuplicates();

    void turnOnCamera(size_t index);
    void turnOffCamera(size_t index);
    void showCameraVideo(size_t index){m_cameras[index]->Show();}
    void hideCameraVideo(size_t index){m_cameras[index]->Hide();}

    void captureAnimationStart();
    Animation *captureAnimationStop();
    void recordingStart();
    void recordingStop();

    void setDimensions(glm::vec3 dims);
    void setName(QString name){this->m_name = name;}
    void setEpsilon(float size);
    void setNumberOfPoints(size_t nOfPts);

    QString getName() const {return m_name;}
    glm::vec3 getDimensions() const {return m_roomDimensions;}
    int getWidth()const {return m_roomDimensions.x;}
    int getLength()const {return m_roomDimensions.y;}
    float getEpsilon() const {return m_maxError;}
    bool getSaved() const {return m_saved;}
    QVector<QVector<Line>> getLines() const {return m_resultLines;}
    std::vector <CaptureCamera*> getcameras()const {return m_cameras;}

    void setOpenglWindow(OpenGLWindow * opengl) {this->m_openGLWindow = opengl;}

    static void Intersection(Edge &camsEdge);

    LocalServer *server() const;
    void setServer(LocalServer *server);

signals:
    void startWork();
    void stopWork();
    void startWork2D();

private slots:
    void resultReady(QVector<Line> lines);
    void record2D();

private:
    void intersections();
    void normaliseCoords(std::vector<Pnt> &m_points3D, glm::vec3 roomSize);

    void weldPoints(std::vector<glm::vec3> &m_points3D);

};

#endif // Room_H
