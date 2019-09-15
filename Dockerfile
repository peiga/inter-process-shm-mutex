FROM gcc:4.9

COPY . /usr/src/app

WORKDIR /usr/src/app

RUN make clean
RUN make

CMD ["./binary"]
