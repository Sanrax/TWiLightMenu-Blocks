FROM skylyrac/blocksds:slim-v1.24.0
RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 g++ p7zip-full \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /data
