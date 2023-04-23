# Docker how to

拉取centos6/centos7 docker image:  docker pull centos:7

进入docker容器： docker run -it -v {OPENOCD_DIR}:/mnt/al_openocd centos:7

在docker shell中执行安装工具链：

yum install epel-release -y

curl -o /etc/yum.repos.d/CentOS-Base.repo https://mirrors.aliyun.com/repo/Centos-7.repo

curl -o /etc/yum.repos.d/epel.repo http://mirrors.myhuaweicloud.com/repo/epel-7.repo

yum clean all

yum update cache

yum install scl-utils centos-release-scl devtoolset-9 libusb libtool file which

注意：需要格外的从Centos8源中升级automake

  112  curl http://repo.okay.com.mx/centos/7/x86_64/release/automake-1.14-1.el7.x86_64.rpm > automake.rpm

  114  yum remove automake

  113  rpm -ivh automake.rpm

进入openocd目录，开始尝试编译

编译通过后commit容器：docker commit {IMAGE_ID} openocd_buildbot:linux