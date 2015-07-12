#include "polygoncameratopology.h"

#include <QDebug>
#include <QtConcurrent>

double PolygonCameraTopology::m_maxError;
QMap<ICamera*, QVector<Line>> PolygonCameraTopology::m_resultLines;

PolygonCameraTopology::PolygonCameraTopology(RoomSettings *settings, QObject *parent) : ICameraTopology(parent)
{
    m_roomSettings = settings;

    connect(m_roomSettings, &RoomSettings::changed, this, &PolygonCameraTopology::handleRoomSettingsChange);

    m_waitCondition = new QWaitCondition;
}

QVariantMap PolygonCameraTopology::toVariantMap()
{
    QVariantMap retVal;

    return retVal;
}

void PolygonCameraTopology::fromVariantMap(QVariantMap varMap)
{

}

void PolygonCameraTopology::addCameras(QVector<ICamera*> cameras)
{
    foreach (ICamera* cam, cameras) {

        cam->setWaitCondition(m_waitCondition);

        if(cam->settings()->turnedOn())
        {
            ++m_turnedOnCamerasCounter;
        }

        m_cameras.push_back(cam);

        QThread *thread = new QThread();
        cam->moveToThread(thread);
        m_cameraThreads.push_back(thread);

        ///connect( this, SIGNAL(startWork2D()), this, SLOT(record2D()));
        connect(this, &PolygonCameraTopology::startRecording, cam, &ICamera::startWork);
        connect(this, &PolygonCameraTopology::stopRecording, cam, &ICamera::stopWork);
        connect(cam, &ICamera::results,this,&PolygonCameraTopology::handleCameraResults, Qt::QueuedConnection);
        connect(cam, &ICamera::finished, thread, &QThread::quit);
        connect(cam, &ICamera::finished, cam, &ICamera::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);

        thread->start();
        /*
         *
    workers.push_back(new worker(&m_linesWaitCondition, cam));

         * */
    }

    resolveEdges();
}

void PolygonCameraTopology::removeCamera(ICamera *camera)
{

}

void PolygonCameraTopology::record(bool start)
{
    if(start)
    {
        emit startRecording();
        m_frameTimer.start();
    }
    else
    {
        emit stopRecording();
    }
}

void PolygonCameraTopology::resolveEdges()
{
    m_topology.clear();

    QVector3D pos1, pos2;
    QVector3D dir1, dir2;

    float min = 181.0f, temp_angle;
    int min_index = -1;

    //get
    for(int i = 0; i < m_cameras.size(); i++)
    {

        pos1 = m_cameras[i]->settings()->globalPosition();
        dir1 = m_cameras[i]->settings()->getDirectionVector().toVector3D();

        for(int j = i+1; j < m_cameras.size(); j++)
        {

            pos2 = m_cameras[j]->settings()->globalPosition();
            dir2 = m_cameras[j]->settings()->getDirectionVector().toVector3D();

            temp_angle = Line::lineAngle(QVector2D(dir1.x(), dir1.y()),QVector2D(dir2.x(), dir2.y()));

            if(qAbs(temp_angle) < min)
            {
                min = temp_angle;
                min_index = j;
            }
        }

        if(min_index != -1)
        {
            TopologyEdge edge(m_cameras[i],m_cameras[min_index]);

            m_topology.push_back(edge);
        }
    }

    for(int i = 0; i < m_topology.size(); i++)
    {
        ICamera* index1 = m_topology[i].m_camera1;
        ICamera* index2 = m_topology[i].m_camera2;

        for(int j = 0; j < m_topology.size(); j++)
        {
            if(m_topology[j].m_camera2 == index2 && m_topology[j].m_camera1 == index1)
            {
                m_topology.erase(m_topology.begin()+j);
            }
        }
    }

    std::cout << "new camtopology size:" << m_topology.size() << std::endl;
}

void PolygonCameraTopology::intersections()
{
    QVector<QVector<QVector3D>> points = QtConcurrent::blockingMapped<QVector<QVector<QVector3D>>>(m_topology, PolygonCameraTopology::intersection);

    QVector<QVector3D> pointsFlatten;

    for(auto pts: points)
    {
        pointsFlatten.append(pts);
    }

    auto labeledPoints = m_pointChecker.solvePointIDs(pointsFlatten);

    normaliseCoords(labeledPoints, m_roomSettings->roomDimensions());

    emit frameReady(Frame(m_frameTimer.elapsed(), labeledPoints, m_resultLines.values().toVector()));

    QCoreApplication::processEvents();
}

void PolygonCameraTopology::normaliseCoords(QVector<Marker> &points, QVector3D roomSize)
{
    for(Marker &pnt: points)
    {
        auto position = pnt.position();

        pnt.setPosition({position.x() / roomSize.x() , position.y() / roomSize.y(), position.z() / roomSize.z()});
    }
}

QVector<QVector3D> PolygonCameraTopology::intersection(TopologyEdge edge)
{
    QVector<QVector3D> retVal;

    for(int i = 0; i < m_resultLines[edge.m_camera1].size(); i++)
    {
        for(int j = 0; j < m_resultLines[edge.m_camera2].size(); j++)
        {
            QVector3D tempPoint;
            if(Line::intersection(m_resultLines[edge.m_camera1][i], m_resultLines[edge.m_camera2][j], m_maxError, tempPoint))
            {
                retVal.push_back(tempPoint);
            }
        }
    }

    return retVal;
}

void PolygonCameraTopology::handleCameraSettingsChange(CameraSettings::CameraSettingsType type)
{
    CameraSettings *settings = qobject_cast<CameraSettings*>(sender());

    switch (type) {
    case CameraSettings::CameraSettingsType::TURNEDON:
        if(settings->turnedOn())
        {
            ++m_turnedOnCamerasCounter;
        }
        else
        {
            --m_turnedOnCamerasCounter;
        }
        break;
    default:
        break;
    }
}

void PolygonCameraTopology::handleRoomSettingsChange(RoomSettings::RoomSettingsType type)
{
    auto roomSettings = qobject_cast<RoomSettings*>(sender());

    switch (type) {
    case RoomSettings::RoomSettingsType::MAXERROR:
        m_maxError = roomSettings->maxError();
        break;
    case RoomSettings::RoomSettingsType::DIMENSIONS:
        //m_roomDims = roomSettings->roomDimensions();
        break;
    default:
        break;
    }
}

void PolygonCameraTopology::handleCameraResults(QVector<Line> lines)
{
    qDebug() << "results size: " << lines.size();

    cv::waitKey(1);

    ++m_resultsCounter;

    m_resultLines[qobject_cast<ICamera*>(sender())] = lines;

    /*
    QObject *obj = QObject::sender();

    for(size_t i = 0; i < workers.size(); i++)
    {
        if(obj == workers[i])
        {
            if(m_haveResults[i])
            {
                qDebug() << "bad sync camera " << i;
            }

            m_haveResults[i] = true;
            for(size_t j = 0; j < m_cameraTopology.size(); j++)
            {
                if(m_cameraTopology[j].m_index1 == i)
                {
                    m_cameraTopology[j].a = lines;
                }

                if(m_cameraTopology[j].m_index2 == i)
                {
                    m_cameraTopology[j].b = lines;
                }

                m_resultLines[i] = lines;
            }

            break;
        }
    }


    for(size_t i = 0; i < workers.size(); i++)
    {
        if(!m_haveResults[i])
        {
            return;
        }
    }

    */

    if(m_resultsCounter == m_cameras.size())
    {
        intersections();

        qDebug() << m_frameTimer.elapsed();

        m_frameTimer.restart();
        m_waitCondition->wakeAll();

        m_resultsCounter = 0;
    }

    QCoreApplication::processEvents();
}
