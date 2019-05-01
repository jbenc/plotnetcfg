FROM centos:7 

COPY . /plotnetcfg

WORKDIR /plotnetcfg

RUN yum install -y make jansson-devel gcc 

RUN make 

FROM centos:7

RUN yum -y install jansson

COPY --from=0 /plotnetcfg/plotnetcfg /bin/

CMD ["/bin/plotnetcfg"]
