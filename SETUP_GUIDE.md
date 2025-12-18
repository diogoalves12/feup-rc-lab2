# Guia de Configuração - Projeto 2

## Índice
- [Identificação dos Equipamentos](#identificação-dos-equipamentos)
- [Experiência 1: Configurar uma Rede IP](#experiência-1-configurar-uma-rede-ip)
- [Experiência 2: Implementar Bridges](#experiência-2-implementar-bridges)
- [Experiência 3: Configurar um Router](#experiência-3-configurar-um-router)
- [Experiência 4: Configurar um Router em Linux](#experiência-4-configurar-um-router-em-linux)
- [Experiência 5: DNS](#experiência-5-dns)
- [Experiência 6: Conectar ao FTP](#experiência-6-conectar-ao-ftp)

---

## Identificação dos Equipamentos

| Equipamento | Interface | Descrição |
|-------------|-----------|-----------|
| Tux83 | ether1 | Tux83e1 |
| Tux84 | ether2 | Tux84e1 |
| Tux82 | ether4 | Tux82e1 |
| Tux84 | ether8 | Tux84e2 |
| Router | ether18 | Rc |

---

## Experiência 1: Configurar uma Rede IP

### Objetivo
Configurar uma rede IP básica entre dois computadores (Tux83 e Tux84).

### Reset Inicial
```bash
/system reset-config
```

### Passos

#### 1. Configurar Tux83 (PC3)
```bash
sudo ifconfig if_e1 172.16.80.1/24
```

#### 2. Configurar Tux84 (PC4)
```bash
sudo ifconfig if_e1 172.16.80.254/24
```

#### 3. Testar Conectividade
```bash
# No Tux83
ping 172.16.80.254
```

#### 4. Verificar Configuração (Opcional)
```bash
sudo route -n
sudo arp -a
```

#### 5. Limpar Cache ARP (Se necessário)
```bash
sudo arp -d <ipaddressTux84>
```

---

## Experiência 2: Implementar Bridges

### Objetivo
Criar duas bridges (bridge80 e bridge81) e conectar os computadores através delas.

### Passos

#### 1. Configurar Tux82 (PC2)
```bash
sudo ifconfig if_e1 172.16.81.1/24
```

#### 2. Criar Bridges no Router (via GTKTerm - admin)
```bash
/interface bridge add name=bridge80
/interface bridge add name=bridge81
```

#### 3. Remover Interfaces Existentes
```bash
/interface bridge port remove [find interface=ether1]
/interface bridge port remove [find interface=ether2]
/interface bridge port remove [find interface=ether4]
```

#### 4. Adicionar Interfaces às Bridges
```bash
# Bridge 80 - Rede 172.16.80.0/24
/interface bridge port add bridge=bridge80 interface=ether1
/interface bridge port add bridge=bridge80 interface=ether2

# Bridge 81 - Rede 172.16.81.0/24
/interface bridge port add bridge=bridge81 interface=ether4
```

---

## Experiência 3: Configurar um Router

### Objetivo
Configurar o Tux84 como router entre as duas redes.

### Passos

#### 1. Configurar Segunda Interface do Tux84 (PC4)
```bash
sudo ifconfig if_e2 172.16.81.253/24
```

#### 2. Ativar IP Forwarding no Tux84
```bash
sudo sysctl net.ipv4.ip_forward=1
sudo sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
```

#### 3. Adicionar Tux84 à Bridge81 (via GTKTerm)
```bash
/interface bridge port remove [find interface=ether8]
/interface bridge port add bridge=bridge81 interface=ether8
```

#### 4. Configurar Rotas nos Computadores

##### No Tux83 (PC3)
```bash
sudo route add -net 172.16.81.0/24 gw 172.16.80.254
```

##### No Tux82 (PC2)
```bash
sudo route add -net 172.16.80.0/24 gw 172.16.81.253
```

#### 5. Testar Conectividade no Tux83
```bash
ping 172.16.81.1
ping 172.16.81.253
ping 172.16.80.254
```

---

## Experiência 4: Configurar um Router em Linux

### Objetivo
Configurar um router comercial (Rc) para conectar a rede interna à rede externa.

### Passos

#### 1. Adicionar Router à Bridge81 (via GTKTerm)
```bash
/interface bridge port remove [find interface=ether18]
/interface bridge port add bridge=bridge81 interface=ether18
```

#### 2. Mudar Cabo para Router
**Nota:** Efetuar a troca física do cabo de rede.

#### 3. Resetar e Configurar Router (via GTKTerm - admin)
```bash
/system reset-configuration
/ip address add address=172.16.1.81/24 interface=ether1
/ip address add address=172.16.81.254/24 interface=ether2
```

#### 4. Configurar Rotas nos Computadores

##### No Tux82 (PC2)
```bash
sudo route add -net 172.16.1.0/24 gw 172.16.81.254
```

##### No Tux84 (PC4)
```bash
sudo route add -net 172.16.1.0/24 gw 172.16.81.254
```

##### No Tux83 (PC3)
```bash
sudo route add -net 172.16.1.0/24 gw 172.16.80.254
```

#### 5. Configurar Rota no Router (via GTKTerm - admin)
```bash
/ip route add dst-address=172.16.80.0/24 gateway=172.16.81.253
```

#### 6. Testar Conectividade no Tux83
```bash
ping 172.16.80.254
ping 172.16.81.1
ping 172.16.81.254
ping 172.16.1.10
```

---

## Experiência 5: DNS

### Objetivo
Configurar DNS para resolução de nomes.

### Passos

#### 1. Configurar DNS em Todos os Computadores (PC2, PC3, PC4)
```bash
sudo nano /etc/resolv.conf
```

#### 2. Adicionar Nameserver
```
nameserver 10.227.20.3
```

#### 3. Testar Resolução DNS
```bash
ping ftp.netlab.fe.up.pt
```

---

## Experiência 6: Conectar ao FTP

### Objetivo
Estabelecer conexão com o servidor FTP externo.

### Verificação Final
```bash
# Em todos os 3 PCs
ping ftp.netlab.fe.up.pt
```

Se todos os pings funcionarem, a configuração está completa e o sistema está pronto para realizar download via FTP.

---

## Diagrama de Rede

```
┌──────────┐
│  Tux83   │ 172.16.80.1/24 (e1)
│  (PC3)   │
└─────┬────┘
      │
   [bridge80]
      │
┌─────┴────┐ 172.16.80.254/24 (e1)
│  Tux84   ├───────────────────────┐
│  (PC4)   │ 172.16.81.253/24 (e2) │
└──────────┘                        │
                                 [bridge81]
                                    │
                      ┌─────────────┼─────────────┐
                      │             │             │
                ┌─────┴────┐   ┌────┴────┐   ┌────┴────┐
                │  Tux82   │   │  Router │   │         │
                │  (PC2)   │   │  (Rc)   │   │ Externa │
                └──────────┘   └─────────┘   └─────────┘
           172.16.81.1/24   172.16.81.254/24  172.16.1.0/24
                (e1)         172.16.1.81/24
```

---

## Notas Importantes

- **if_e1** e **if_e2** referem-se às interfaces de rede específicas de cada computador
- As configurações devem ser executadas na ordem apresentada
- Comandos com **GTKTerm (admin)** devem ser executados no terminal do router
- Comandos com **cmd** devem ser executados no terminal dos computadores
- Todas as configurações de rotas são essenciais para a comunicação entre redes diferentes
