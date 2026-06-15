/**
 * SmartGuard 智能门卫 - 门口人数监控 + SRS推流
 * 编译: make -f Makefile_smart
 */
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QTextEdit>
#include <QDateTime>
#include <QLineEdit>
#include <QMessageBox>
#include <QCloseEvent>
#include <QProcess>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <fstream>
#include <vector>
#include <deque>
#include <string>
#include <ctime>
#include "v4l2_camera.h"

#define ADMIN_USER "admin"
#define ADMIN_PASS "123456"

int person_count=0,last_count=-1,peak_today=0;
double presence_start=0;
std::deque<int> history;

cv::dnn::Net* g_net=nullptr;
cv::dnn::Net* face_net=nullptr;

std::vector<cv::Rect> detect(cv::Mat& f){
    std::vector<cv::Rect> r;
    if(face_net){
        cv::Mat b=cv::dnn::blobFromImage(f,1.0,cv::Size(300,300),cv::Scalar(104,177,123),false,false);
        face_net->setInput(b);cv::Mat d=face_net->forward();
        int N=d.size[2];float fh=f.rows,fw=f.cols;
        for(int i=0;i<N;i++){float* p=(float*)d.ptr()+i*7;
            if(p[2]>0.5){int x1=p[3]*fw,y1=p[4]*fh,x2=p[5]*fw,y2=p[6]*fh;
                r.push_back(cv::Rect(x1,y1,x2-x1,y2-y1));}}
    }else{
        cv::Mat b=cv::dnn::blobFromImage(f,0.007843,cv::Size(300,300),127.5);
        g_net->setInput(b);cv::Mat d=g_net->forward();
        int N=d.size[2];float fh=f.rows,fw=f.cols;
        for(int i=0;i<N;i++){float* p=(float*)d.ptr()+i*7;
            if(p[2]>0.5&&(int)p[1]==15){int x1=p[3]*fw,y1=p[4]*fh,x2=p[5]*fw,y2=p[6]*fh;
                r.push_back(cv::Rect(x1,y1,x2-x1,y2-y1));}}
    }
    return r;
}

class LoginDialog:public QDialog{
    Q_OBJECT
public:
    LoginDialog(QWidget* p=nullptr):QDialog(p){
        setWindowTitle("SmartGuard");setFixedSize(380,300);
        setStyleSheet("QDialog{background:#0a120e;}QLabel{color:#c8dcc8;font-size:13px;}"
            "QLineEdit{background:#0d1a10;color:#a0e8a0;border:2px solid #1a3a20;border-radius:10px;padding:12px;font-size:15px;}"
            "QPushButton{background:#182818;color:#b0e8b0;border:1px solid #2a4a2a;border-radius:10px;padding:12px;font-size:14px;font-weight:bold;}"
            "QPushButton:hover{background:#1e341e;border-color:#50d060;}");
        QVBoxLayout* l=new QVBoxLayout(this);l->setSpacing(12);l->setContentsMargins(36,28,36,28);
        QLabel* t=new QLabel("SmartGuard");t->setAlignment(Qt::AlignCenter);
        t->setStyleSheet("font-size:26px;font-weight:900;color:#30e030;");l->addWidget(t);
        QLabel* s=new QLabel("智能门卫监控系统");s->setAlignment(Qt::AlignCenter);
        s->setStyleSheet("font-size:13px;color:#60a060;");l->addWidget(s);
        user_=new QLineEdit();user_->setPlaceholderText("用户名");l->addWidget(user_);
        pass_=new QLineEdit();pass_->setPlaceholderText("密码");pass_->setEchoMode(QLineEdit::Password);l->addWidget(pass_);
        QPushButton* b=new QPushButton("进入系统");l->addWidget(b);
        connect(b,&QPushButton::clicked,this,&LoginDialog::check);
        connect(pass_,&QLineEdit::returnPressed,this,&LoginDialog::check);
    }
private slots:
    void check(){if(user_->text()==ADMIN_USER&&pass_->text()==ADMIN_PASS)accept();
        else{QMessageBox::warning(this,"错误","用户名或密码错误");pass_->clear();pass_->setFocus();}}
private:
    QLineEdit *user_,*pass_;
};

class SmartGuard:public QMainWindow{
    Q_OBJECT
public:
    SmartGuard(V4L2Camera* cam):cam_(cam){
        setWindowTitle("SmartGuard");resize(940,560);setMinimumSize(760,480);
        setStyleSheet("QMainWindow{background:#0d1117;}QLabel{color:#c9d1d9;}"
            "QPushButton{background:#21262d;color:#e6edf3;border:1px solid #30363d;border-radius:8px;padding:10px 16px;font-size:13px;font-weight:600;}"
            "QPushButton:hover{background:#292e36;border-color:#58a6ff;}QPushButton:pressed{background:#161b22;}"
            "QTextEdit{background:#0d1117;color:#7ee787;border:1px solid #21262d;border-radius:8px;font-size:11px;font-family:monospace;padding:8px;}");

        QWidget* cw=new QWidget();setCentralWidget(cw);
        QHBoxLayout* ml=new QHBoxLayout();ml->setContentsMargins(12,12,12,12);ml->setSpacing(12);cw->setLayout(ml);

        QVBoxLayout* left=new QVBoxLayout();left->setSpacing(6);
        video_=new QLabel();video_->setScaledContents(true);
        video_->setStyleSheet("background:#000;border:1px solid #21262d;border-radius:12px;");
        left->addWidget(video_,1);

        QHBoxLayout* bar=new QHBoxLayout();
        blink_dot_=new QLabel("");blink_dot_->setFixedSize(10,10);
        blink_dot_->setStyleSheet("background:#3fb950;border-radius:5px;");bar->addWidget(blink_dot_);
        QLabel* live=new QLabel("LIVE");live->setStyleSheet("font-size:10px;font-weight:bold;color:#3fb950;");bar->addWidget(live);
        bar->addSpacing(12);
        stream_dot_=new QLabel("");stream_dot_->setFixedSize(8,8);
        stream_dot_->setStyleSheet("background:#484f58;border-radius:4px;");bar->addWidget(stream_dot_);
        bar->addWidget(new QLabel("RTMP"));bar->addSpacing(12);
        onenet_dot_=new QLabel("");onenet_dot_->setFixedSize(8,8);
        onenet_dot_->setStyleSheet("background:#484f58;border-radius:4px;");bar->addWidget(onenet_dot_);
        bar->addWidget(new QLabel("OneNET"));bar->addSpacing(12);
        status_=new QLabel("就绪");status_->setStyleSheet("font-size:11px;color:#8b949e;");bar->addWidget(status_);
        bar->addStretch();
        clock_label_=new QLabel();clock_label_->setStyleSheet("font-size:11px;color:#58a6ff;");bar->addWidget(clock_label_);
        left->addLayout(bar);ml->addLayout(left,3);

        QVBoxLayout* right=new QVBoxLayout();right->setSpacing(8);
        QLabel* title=new QLabel("SmartGuard");title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size:20px;font-weight:800;color:#f0f6fc;");right->addWidget(title);

        card_=new QFrame();
        card_->setStyleSheet("QFrame{background:#161b22;border-radius:14px;border:1px solid #21262d;}");
        QVBoxLayout* ccl=new QVBoxLayout();ccl->setContentsMargins(0,24,0,24);card_->setLayout(ccl);
        num_=new QLabel("0");num_->setAlignment(Qt::AlignCenter);
        num_->setStyleSheet("font-size:56px;font-weight:800;color:#58a6ff;border:none;");ccl->addWidget(num_);
        QLabel* nl=new QLabel("检测人数");nl->setStyleSheet("font-size:13px;color:#8b949e;");ccl->addWidget(nl);
        right->addWidget(card_);

        QFrame* info=new QFrame();
        info->setStyleSheet("QFrame{background:#161b22;border-radius:10px;border:1px solid #21262d;}");
        QHBoxLayout* il=new QHBoxLayout();il->setContentsMargins(14,10,14,10);info->setLayout(il);
        peak_label_=new QLabel("峰值 0");peak_label_->setStyleSheet("font-size:14px;color:#d2a8ff;");il->addWidget(peak_label_);
        il->addStretch();time_label_=new QLabel("");time_label_->setStyleSheet("font-size:12px;color:#8b949e;");il->addWidget(time_label_);
        right->addWidget(info);

        QLabel* cht=new QLabel("人数趋势");cht->setStyleSheet("font-size:13px;font-weight:bold;color:#f0f6fc;");right->addWidget(cht);
        chart_=new QTextEdit();chart_->setReadOnly(true);chart_->setMaximumHeight(100);right->addWidget(chart_);

        QLabel* lt=new QLabel("事件日志");lt->setStyleSheet("font-size:13px;font-weight:bold;color:#f0f6fc;");right->addWidget(lt);
        log_=new QTextEdit();log_->setReadOnly(true);right->addWidget(log_,1);

        QPushButton* rst=new QPushButton("重置计数");
        connect(rst,&QPushButton::clicked,this,[this](){person_count=0;peak_today=0;history.clear();});
        right->addWidget(rst);ml->addLayout(right,2);

        // ffmpeg 推流管道
        ffmpeg_=new QProcess(this);
        QStringList fa;fa<<"-y"<<"-f"<<"rawvideo"<<"-vcodec"<<"rawvideo"
            <<"-s"<<"320x240"<<"-pix_fmt"<<"bgr24"<<"-r"<<"15"<<"-i"<<"-"
            <<"-c:v"<<"libx264"<<"-preset"<<"ultrafast"<<"-tune"<<"zerolatency"
            <<"-b:v"<<"600k"<<"-maxrate"<<"600k"<<"-bufsize"<<"1200k"
            <<"-g"<<"30"<<"-keyint_min"<<"30"<<"-an"<<"-f"<<"flv"
            <<"rtmp://YOUR_SRS_SERVER_IP/live/camera";
        ffmpeg_->start("ffmpeg",fa);
        stream_ok_=ffmpeg_->waitForStarted(3000);

        blink_on_=true;blink_timer_=new QTimer(this);
        connect(blink_timer_,&QTimer::timeout,this,[this](){blink_on_=!blink_on_;
            blink_dot_->setStyleSheet(blink_on_?"background:#3fb950;border-radius:5px;":"background:#0a2a0a;border-radius:5px;");});
        blink_timer_->start(800);

        QTimer* ct=new QTimer(this);connect(ct,&QTimer::timeout,this,[this](){clock_label_->setText(QDateTime::currentDateTime().toString("hh:mm:ss"));});ct->start(1000);
        QTimer* ot=new QTimer(this);connect(ot,&QTimer::timeout,this,[this](){
            std::ifstream f("/tmp/onenet_status.json");
            if(f.good()){std::string c((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
                bool on=c.find("\"online\": true")!=std::string::npos||c.find("\"online\":true")!=std::string::npos;
                onenet_dot_->setStyleSheet(on?"background:#00d4ff;border-radius:4px;":"background:#484f58;border-radius:4px;");}});
        ot->start(3000);

        timer_=new QTimer(this);connect(timer_,&QTimer::timeout,this,&SmartGuard::tick);timer_->start(66); // ~15 FPS
        log_->append(QString("[%1] SmartGuard 启动%2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
            .arg(stream_ok_?" +推流":""));
    }

private slots:
    void tick(){
        cv::Mat f;if(!cam_->getFrame(f))return;

        // 推流：640x480 → 320x240 缩小后推送
        static cv::Mat small;
        if(stream_ok_ && ffmpeg_->state()==QProcess::Running){
            cv::resize(f,small,cv::Size(320,240));
            ffmpeg_->write((const char*)small.data,small.total()*small.elemSize());
        }

        // AI 每3帧跑一次(~5 FPS)，节省CPU
        static int aic=0;
        static int cached_persons=0;
        if(++aic%3==0){
            auto ps=detect(f);cached_persons=(int)ps.size();
            for(auto& p:ps)cv::rectangle(f,p,cv::Scalar(0,220,60),2);
        }
        person_count=cached_persons;
        double now=time(nullptr);
        if(person_count>0){if(presence_start==0)presence_start=now;if(person_count>peak_today)peak_today=person_count;}
        else{presence_start=0;}
        int timing_val=person_count>0?(int)(now-presence_start):0;

        cv::cvtColor(f,f,cv::COLOR_BGR2RGB);
        video_->setPixmap(QPixmap::fromImage(QImage(f.data,f.cols,f.rows,f.step,QImage::Format_RGB888)));

        bool alarm=timing_val>30;
        num_->setText(alarm?"!":QString::number(person_count));
        num_->setStyleSheet(QString("font-size:56px;font-weight:800;color:%1;border:none;")
            .arg(alarm?"#ff4444":person_count>0?"#58a6ff":"#484f58"));
        peak_label_->setText(QString("峰值:%1人 | 停留:%2秒%3").arg(peak_today).arg(timing_val)
            .arg(alarm?" ALERT!":""));
        time_label_->setText(QString("更新:%1").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

        static bool alarm_blink=false;static double last_blink=0;
        if(alarm&&now-last_blink>0.5){alarm_blink=!alarm_blink;last_blink=now;}
        card_->setStyleSheet(QString("QFrame{background:%1;border-radius:14px;border:1px solid %2;}")
            .arg(alarm?(alarm_blink?"#3d1111":"#1a0a0a"):"#161b22")
            .arg(alarm?"#ff4444":"#21262d"));

        static int ls=0;
        if((int)now-ls>30){history.push_back(person_count);if(history.size()>60)history.pop_front();ls=now;}
        QString c="";for(int i=0;i<(int)history.size();i++)c+=(i%10==0)?QString::number(history[i]):".";
        chart_->setText("人数趋势(30s):\n"+c);

        if(person_count!=last_count){log_->append(QString("[%1] %2 人").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(person_count));last_count=person_count;}

        std::ofstream sf("/tmp/seat_state.json");
        sf<<"{\"state\":\""<<(person_count>0?"occupied":"free")<<"\",\"timer\":"<<timing_val<<",\"persons\":"<<person_count<<"}";sf.close();

        // 推流指示灯
        bool st=(ffmpeg_->state()==QProcess::Running);
        stream_dot_->setStyleSheet(st?"background:#ff9933;border-radius:4px;":"background:#484f58;border-radius:4px;");

        status_->setText(alarm?QString("停留%1秒!").arg(timing_val):
            person_count>0?QString("检测%1人").arg(person_count):"无人");
    }

    void closeEvent(QCloseEvent* e){
        if(ffmpeg_){ffmpeg_->closeWriteChannel();ffmpeg_->waitForFinished(2000);}
        if(cam_)cam_->stop();e->accept();
    }
private:
    V4L2Camera* cam_;QLabel *video_,*status_,*num_,*blink_dot_,*onenet_dot_,*clock_label_,*peak_label_,*time_label_,*stream_dot_;
    QTextEdit *log_,*chart_;QTimer *timer_,*blink_timer_;bool blink_on_;QFrame* card_;
    QProcess* ffmpeg_;bool stream_ok_;
};

int main(int argc,char**argv){
    QApplication app(argc,argv);app.setStyle("Fusion");
    std::ifstream ft("/home/orangepi/Desktop/face_deploy.prototxt");
    std::ifstream fm("/home/orangepi/Desktop/face_detector.caffemodel",std::ios::binary|std::ios::ate);
    if(ft.good()&&fm.good()&&fm.tellg()>10000){ft.close();fm.close();
        face_net=new cv::dnn::Net(cv::dnn::readNetFromCaffe(
            "/home/orangepi/Desktop/face_deploy.prototxt",
            "/home/orangepi/Desktop/face_detector.caffemodel"));}
    g_net=new cv::dnn::Net(cv::dnn::readNetFromCaffe(
        "/home/orangepi/Desktop/MobileNetSSD_deploy.prototxt",
        "/home/orangepi/Desktop/MobileNetSSD_deploy.caffemodel"));
    LoginDialog login;if(login.exec()!=QDialog::Accepted)return 0;
    V4L2Camera cam("/dev/video8",640,480);
    if(!cam.init()||!cam.start()){fprintf(stderr,"Camera fail\n");return 1;}
    SmartGuard w(&cam);w.show();
    return app.exec();
}
#include "smartguard.moc"
