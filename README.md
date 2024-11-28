## Instructions to Install Docker and Run the Game

### Step 1: Install Docker

#### For Ubuntu:
1. Update your existing list of packages:
    ### For Ubuntu:
    ```sh
    sudo apt update
    ```

    ### For Windows:
    1. Download Docker Desktop from the official Docker website: [Docker Desktop for Windows](https://www.docker.com/products/docker-desktop)
    2. Run the installer and follow the on-screen instructions.
    3. Once the installation is complete, start Docker Desktop from the Start menu.
    4. Follow the tutorial in Docker Desktop to finish the setup.

    ## Project: Fun Paintball
    This project is for the Tecnológico de Monterrey class "Pensamiento computacional orientado a objetos (Gpo 302)"

    ## Team 2 Members:
    - Daniel Eleazar Fragoso Reyes
    - Francesca Renata García Romero
    - Luis De los Santos Cadena
    - Natalia Sosa Torres

2. Install a few prerequisite packages which let apt use packages over HTTPS:
    ```sh
    sudo apt install apt-transport-https ca-certificates curl software-properties-common
    ```

3. Add the GPG key for the official Docker repository to your system:
    ```sh
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
    ```

4. Add the Docker repository to APT sources:
    ```sh
    sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
    ```

5. Update your existing list of packages again for the new Docker repository:
    ```sh
    sudo apt update
    ```

6. Make sure you are about to install from the Docker repository instead of the default Ubuntu repository:
    ```sh
    apt-cache policy docker-ce
    ```

7. Finally, install Docker:
    ```sh
    sudo apt install docker-ce
    ```

8. Verify that Docker is installed correctly by running:
    ```sh
    sudo systemctl status docker
    ```

#### For Windows and macOS:
Follow the instructions on the official Docker website to download and install Docker Desktop:
[Docker Desktop for Windows and macOS](https://www.docker.com/products/docker-desktop)

### Step 2: Install Docker Compose

1. Download the current stable release of Docker Compose:
    ```sh
    sudo curl -L "https://github.com/docker/compose/releases/download/1.29.2/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    ```

2. Apply executable permissions to the binary:
    ```sh
    sudo chmod +x /usr/local/bin/docker-compose
    ```

3. Verify that Docker Compose is installed correctly by running:
    ```sh
    docker-compose --version
    ```

    **Note:** In recent installations, you can also use `docker compose` instead of `docker-compose`.

### Step 3: Build and Run the Game

1. Navigate to the directory containing the `docker-compose.yml` file.

2. Build and start the Docker containers in detached mode:
    ```sh
    docker-compose up -d --build
    ```

    **Note:** You can also use `docker compose up -d --build` in recent installations.

3. Once the containers are up and running, execute the game inside the `juego_container`:
    ```sh
    docker exec -it juego_container ./juego
    ```

4. Follow the on-screen instructions to play the game.

5. If you are running on a macOS environment, use the `compile_and_run.sh` script to execute the game.

### Additional Commands

- To stop the Docker containers:
    ```sh
    docker-compose down
    ```

    **Note:** You can also use `docker compose down` in recent installations.

- To view the logs of the running containers:
    ```sh
    docker-compose logs -f
    ```

    **Note:** You can also use `docker compose logs -f` in recent installations.

- To list all running Docker containers:
    ```sh
    docker ps
    ```

- To remove all stopped containers, unused networks, and dangling images:
    ```sh
    docker system prune -f
    ```

Enjoy the game!
