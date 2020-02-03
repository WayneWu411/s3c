#!/bin/bash -x

file=swipe.dat
bucket=vmms-backup-cashless
resource="/${bucket}/vmms/${file}"
contentType="application/application/octet-stream"
dateValue=`date -R`
stringToSign="PUT\n\n${contentType}\n${dateValue}\n${resource}"
s3Key='AKIAJZEU5M3NEM6MEAQQ'
s3Secret='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'
signature1=`echo -en ${stringToSign}`
signature2='`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary'
signature=`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`
curl -v -X PUT -T "${file}" \
        -H "Host: ${bucket}.s3-ap-southeast-2.amazonaws.com" \
        -H "Date: ${dateValue}" \
        -H "Content-Type: ${contentType}" \
        -H "Authorization: AWS ${s3Key}:${signature}" \
        https://${bucket}.s3-ap-southeast-2.amazonaws.com/vmms/${file}