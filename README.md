# Debian 8 için:
# DOM dersininin labında kullanılan json kütüphanesini kullanıldı. Kurulumu için :
sudo apt-get install libjson0 libjson0-dev

# Debian 9 ve üstünde libjson0 libjson0-dev için hata verirse kurulumu alttaki komutlar ile gerçekleştirebilirsiniz.

sudo apt-get install libjsoncpp-dev <br>
sudo ln -s /usr/include/jsoncpp/json/ /usr/include/json

# Derleme:

gcc 2015510102_server.c -o 2015510102_server -ljson-c -lpthread <br>
gcc 2015510102_client.c -o 2015510102_client -ljson-c -lpthread 
