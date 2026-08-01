#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <vector>
#include <fcntl.h>

#define PORT 8080
#define MAX_CLIENTS 10

int main() {
    // 1. SOCKET OLUŞTURMA
    // AF_INET: IPv4, SOCK_STREAM: TCP protokolü
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket oluşturulamadı!" << std::endl;
        return 1;
    }

    // Soketi non-blocking (bloke olmayan) modda ayarla (ft_irc için çok önemlidir)
    // Bu sayede accept() veya recv() fonksiyonları beklerken program takılıp kalmaz.
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // Portun program kapatıldıktan hemen sonra yeniden kullanılabilmesi için ayar
    // (Geliştirme aşamasında "Address already in use" hatasını önler)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. ADRES YAPILANDIRMASI
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Tüm yerel IP'lerden (0.0.0.0) bağlantı kabul et
    address.sin_port = htons(PORT);       // Port numarasını ağ byte sırasına (Network Byte Order) çevir

    // 3. BIND (BAĞLAMA)
    // Oluşturduğumuz soketi belirlediğimiz IP ve Porta bağlıyoruz
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "Bind işlemi başarısız!" << std::endl;
        close(server_fd);
        return 1;
    }

    // 4. LISTEN (DİNLEME)
    // Sunucuyu gelen bağlantıları kabul etmek üzere dinleme moduna alıyoruz.
    if (listen(server_fd, MAX_CLIENTS) == -1) {
        std::cerr << "Listen işlemi başarısız!" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "Sunucu " << PORT << " portunda dinleniyor..." << std::endl;

    // Birden fazla bağlantıyı takip etmek için pollfd yapılarından oluşan bir liste (vector)
    std::vector<struct pollfd> fds;
    
    // Önce ana sunucu soketini fds listesine ekliyoruz (Yeni bağlantıları yakalamak için)
    struct pollfd server_pollfd;
    server_pollfd.fd = server_fd;
    server_pollfd.events = POLLIN; // POLLIN: Okunabilir bir durum var mı? (Gelen veri veya yeni bağlantı)
    server_pollfd.revents = 0;
    fds.push_back(server_pollfd);

    // Ana Sunucu Döngüsü (Server Loop)
    while (true) {
        // 5. POLL İLE OLAYLARI BEKLEME
        // fds listesindeki soketlerden herhangi birinde bir olay olana kadar bekle (-1 sonsuza kadar bekler)
        int poll_count = poll(fds.data(), fds.size(), -1);
        
        if (poll_count == -1) {
            std::cerr << "Poll hatası!" << std::endl;
            break;
        }

        // fd listesinde gezinip hangi soketlerde hareketlilik olduğunu kontrol et
        for (size_t i = 0; i < fds.size(); i++) {
            
            // Eğer bu sokette herhangi bir olay yoksa bir sonrakine geç
            if (fds[i].revents == 0) continue;

            // Eğer okunabilir bir olay (POLLIN) tetiklendiyse:
            if (fds[i].revents & POLLIN) {
                
                // DURUM A: Olay ANA SUNUCU soketinde olduysa -> Yeni bir müşteri (istemci) kapıyı çalıyor demektir
                if (fds[i].fd == server_fd) {
                    struct sockaddr_in client_address;
                    socklen_t client_len = sizeof(client_address);
                    
                    // 6. ACCEPT (KABUL ET)
                    int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
                    if (client_fd != -1) {
                        std::cout << "Yeni bir istemci bağlandı! FD (Dosya Tanımlayıcı): " << client_fd << std::endl;
                        
                        // Yeni bağlanan istemciyi dinlenecekler listesine ekle
                        struct pollfd client_pollfd;
                        client_pollfd.fd = client_fd;
                        client_pollfd.events = POLLIN; // Artık bu istemciden gelecek mesajları da dinleyeceğiz
                        client_pollfd.revents = 0;
                        fds.push_back(client_pollfd);
                    }
                }
                // DURUM B: Olay BİR İSTEMCİ soketinde olduysa -> İstemci bize bir mesaj (komut) gönderdi demektir
                else {
                    char buffer[1024] = {0}; // Mesajı okuyacağımız tampon bellek
                    
                    // 7. RECV (OKU)
                    int bytes_read = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);
                    
                    if (bytes_read <= 0) {
                        // Eğer okunan byte 0 ise istemci bağlantıyı kesti demektir. Hata varsa da -1 döner.
                        std::cout << "İstemci ayrıldı. FD: " << fds[i].fd << std::endl;
                        close(fds[i].fd); // Soketi kapat
                        fds.erase(fds.begin() + i); // İstemciyi poll listesinden çıkar
                        i--; // Vector'den eleman sildiğimiz için indeksi bir geri alıp kaymayı önlüyoruz
                    } else {
                        // İstemciden gelen mesaj başarıyla okundu
                        std::cout << "Gelen Mesaj (FD " << fds[i].fd << "): " << buffer;
                        
                        // İstemciye (mesajı gönderene) onay mesajı geri gönderiyoruz (SEND)
                        std::string response = "Sunucu mesajini aldi!\n";
                        send(fds[i].fd, response.c_str(), response.size(), 0);
                    }
                }
            }
        }
    }

    // Program biterken sunucu soketini kapat
    close(server_fd);
    return 0;
}
