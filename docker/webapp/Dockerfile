FROM php:7.2-apache

ARG WEB_USER
ARG WEB_GROUP
ARG APACHE_ROOT_DIR
ARG APACHE_WEBROOT

COPY httpd.conf ${APACHE_ROOT_DIR}/conf/httpd.conf

RUN chgrp -R ${WEB_GROUP} ${APACHE_ROOT_DIR}/conf/httpd.conf \
 && usermod -u 1000 ${WEB_USER} \
 && groupmod -g 1000 ${WEB_GROUP} \
 && chgrp -R ${WEB_GROUP} ${APACHE_ROOT_DIR}

RUN docker-php-ext-install pdo_mysql && \
    docker-php-ext-configure mysqli --with-mysqli

EXPOSE ${APACHE_EXPOSED_PORT}
EXPOSE 443
