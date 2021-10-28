#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QMessageBox>
#include <QDebug>
#include <regex>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtWidgets/QLabel>
#include <QtCore/QRandomGenerator>
#include <QtCharts/QValueAxis>


using namespace std;

QSerialPort *m_serialPort = new QSerialPort();//实例化串口类一个对象
QStringList m_serialPortName;
QStringList m_serialPortInfo;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_listCount(3),
      m_valueMax(10),
      m_valueCount(7),
      m_dataTable(generateRandomData(m_listCount, m_valueMax, m_valueCount)),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    SearchCom();

    inAirCut = 0;
    outAirCut = 0;
    airSize = 0;


    //create charts
    QChartView *chartView;
    chartView = new QChartView(createSplineChart());
    ui->gridLayout_charts->addWidget(chartView);
    m_charts << chartView;


    connect(timer, SIGNAL(timeout()), this, SLOT(UpdateState()));//要自定义update函数实现自己的功能哟


}



MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::AirStateUpdate()
{
    static bool airStateFlag = true;

    if(airStateFlag){
        ui->frame_runState->setStyleSheet("border-radius:7px;background-color: rgb(255, 255, 255);");
        qDebug()<<"1.0";
    }
    else{
        ui->frame_runState->setStyleSheet("border-radius:7px;background-color: rgb(255, 0, 127);;");
        qDebug()<<"1.1";
    }
    airStateFlag = !airStateFlag;
}



//搜索串口

void MainWindow::SearchCom(){

    m_serialPortName.clear();
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        m_serialPortName << info.portName();
        m_serialPortInfo << "(" + info.portName() + ")" + info.description();

    }
    if(m_serialPortName.size() != 0){
        ui->comboBox_com->clear();
        ui->comboBox_com->addItems(m_serialPortInfo);

    }
    else{
        QMessageBox::warning(this,"信息","找不到可用的串口信息！！");
    }



}

void MainWindow::OpenCom(){

    if(m_serialPort->isOpen())//如果串口已经打开了 先给他关闭了
    {
        m_serialPort->clear();
        m_serialPort->close();
    }
    else{
        QString portCom = ui->comboBox_com->currentText();

        string strPortCom = portCom.toStdString();
        regex pattern("\\((.*)\\)");  //匹配格式，C++11切记注'\\'代表转义'\' 非常重要！！！
        smatch result;  //暂存匹配结果

        //迭代器声
        string::const_iterator iterStart = strPortCom.begin();
        string::const_iterator iterEnd = strPortCom.end();

        //提取COM口字符串
        if(regex_search(iterStart, iterEnd, result, pattern))
        {
            cout << result.str(1)<<endl;
            portCom = QString::fromStdString(result.str(1));
            m_serialPort->setPortName(portCom);

            if(!m_serialPort->open(QIODevice::ReadWrite))//用ReadWrite 的模式尝试打开串口
            {
                QMessageBox::warning(this,"信息","串口打开失败！！");
                ui->radioButton_switchCom->setChecked(false);
                return;
            }
            else{
                ui->radioButton_switchCom->setText("串口已打开！");
                ui->radioButton_switchCom->setChecked(true);
            }
        }
    }


    m_serialPort->setBaudRate(ui->comboBox_baudrate->currentText().toInt(),QSerialPort::AllDirections);//设置波特率和读写方向
    m_serialPort->setDataBits(QSerialPort::Data8);		//数据位为8位
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);//无流控制
    m_serialPort->setParity(QSerialPort::NoParity);	//无校验位
    m_serialPort->setStopBits(QSerialPort::OneStop); //一位停止位

    //连接信号槽 当下位机发送数据QSerialPortInfo 会发送个 readyRead 信号,我们定义个槽void receiveInfo()解析数据
    connect(m_serialPort,SIGNAL(readyRead()),this,SLOT(ReceiveInfo()));

}



void MainWindow::on_pushButton_scanCom_clicked()
{
    SearchCom();
}


void MainWindow::on_radioButton_switchCom_clicked()
{
    if(ui->radioButton_switchCom->isChecked()){

        OpenCom();
    }
    else{
        CloseCom();
    }

}


void MainWindow::CloseCom(){
    ui->radioButton_switchCom->setChecked(false);
    if(m_serialPort->isOpen())
    {
        m_serialPort->clear();
        m_serialPort->close();
    }
    ui->radioButton_switchCom->setText("串口未打开！");
}




void MainWindow::SendInfo(QByteArray &info){
    if(m_serialPort->isOpen())
    {

        qDebug()<<"Write to serial: "<<info;

        m_serialPort->write(info);//这句是真正的给单片机发数据 用到的是QIODevice::write 具体可以看文档
    }
    else{
        QMessageBox::warning(this,"信息","串口还未打开哎！！");
    }



}

void MainWindow::ReceiveInfo(){
    QByteArray info = m_serialPort->readLine();
    qDebug()<< "info = "<< info;

    ui->textBrowser_receive->setText(info);

    if(info == "#"){
        timer->stop();//设置时间间隔为500毫秒
        ui->frame_runState->setStyleSheet("border-radius:7px;background-color: rgb(85, 255, 0);");
        inAirCut++;
        ui->label_inAirCut->setText(QString::number(inAirCut));
    }
    else if(info == "2"){
        outAirCut++;
        ui->label_outAirCut->setText(QString::number(outAirCut));
    }
    else{
        QMessageBox::warning(this,"信息","操作失败！！");
    }


}




//绘图
DataTable MainWindow::generateRandomData(int listCount, int valueMax, int valueCount) const
{
    DataTable dataTable;

    // generate random data
    for (int i(0); i < listCount; i++) {
        DataList dataList;
        qreal yValue(0);
        for (int j(0); j < valueCount; j++) {
            yValue = yValue + QRandomGenerator::global()->bounded(valueMax / (qreal) valueCount);
            QPointF value((j + QRandomGenerator::global()->generateDouble()) * ((qreal) m_valueMax / (qreal) valueCount),
                          yValue);
            QString label = "Slice " + QString::number(i) + ":" + QString::number(j);
            dataList << Data(value, label);
        }
        dataTable << dataList;
    }

    return dataTable;
}


QChart *MainWindow::createSplineChart() const
{
    QChart *chart = new QChart();
    chart->setTitle("Spline chart");
    QString name("Series ");
    int nameIndex = 0;
    for (const DataList &list : m_dataTable) {
        QSplineSeries *series = new QSplineSeries(chart);
        for (const Data &data : list)
            series->append(data.first);
        series->setName(name + QString::number(nameIndex));
        nameIndex++;
        chart->addSeries(series);
    }

    chart->createDefaultAxes();
    //chart->axes(Qt::Horizontal).first()->setRange(0, m_valueMax);
    //chart->axes(Qt::Vertical).first()->setRange(0, m_valueCount);

    // Add space to label to add space between labels and axis
    QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
    Q_ASSERT(axisY);
    axisY->setLabelFormat("%.1f  ");
    return chart;
}






void MainWindow::on_comboBox_baudrate_currentTextChanged(const QString &arg1)
{
    CloseCom();
}




//充气
void MainWindow::on_pushButton_inAir_clicked()
{
    timer->start(500);//设置时间间隔为500毫秒
    QByteArray orderInfo = "1@";
    SendInfo(orderInfo);



}

//放气
void MainWindow::on_pushButton_outAir_clicked()
{
    QByteArray orderInfo = "2";
    SendInfo(orderInfo);


}

