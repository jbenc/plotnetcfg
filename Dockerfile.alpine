FROM alpine:latest as base

COPY . /plotnetcfg

WORKDIR /plotnetcfg

RUN apk add --update make jansson-dev gcc linux-headers libc-dev 

RUN make 

FROM alpine:latest

RUN apk add --update jansson

COPY --from=base /plotnetcfg/plotnetcfg /bin/
 
CMD ["/bin/plotnetcfg"]
