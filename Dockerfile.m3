FROM sunfleck-m2
RUN apt-get update && apt-get install -y --no-install-recommends libegl-dev libgles-dev libegl-mesa0 && rm -rf /var/lib/apt/lists/*
