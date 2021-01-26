FROM ubuntu:20.04

RUN apt-get update
RUN apt-get install sudo vim python3 iputils-ping openssh-server openssh-client software-properties-common gdb -y
RUN apt-get update
RUN apt-get install cmake cmake-data build-essential uuid-dev pkg-config curl python3-pip -y


# install pip and ansible
#RUN curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
#RUN python3 get-pip.py --user


RUN /usr/bin/ssh-keygen -A

RUN sed -ri 's/^#?PermitRootLogin\s+.*/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed -ri 's/UsePAM yes/#UsePAM yes/g' /etc/ssh/sshd_config

RUN service ssh start

RUN useradd -rm -d /home/dsp -s /bin/bash -g root -G sudo -u 1001 dsp
RUN echo "dsp:dsp" | chpasswd
CMD ["/usr/sbin/sshd","-D"]

USER dsp
WORKDIR /home/dsp

RUN python3 -m pip install --user ansible

USER root

