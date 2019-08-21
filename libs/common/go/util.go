package util

import (
	configmgr "IEdgeInsights/libs/ConfigManager"
	"log"
	"net"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/golang/glog"
)

// CheckPortAvailability - checks for port availability on hostname
func CheckPortAvailability(hostname, port string) bool {
	maxRetries := 1000
	retryCount := 0

	portUp := false
	glog.Infof("Waiting for Port: %s on hostname: %s ", port, hostname)
	for retryCount < maxRetries {
		conn, _ := net.DialTimeout("tcp", net.JoinHostPort(hostname, port), (5 * time.Second))
		if conn != nil {
			glog.Infof("Port: %s on hostname: %s is up.", port, hostname)
			conn.Close()
			portUp = true
			break
		}
		time.Sleep(100 * time.Millisecond)
		retryCount++
	}
	return portUp
}

// GetMessageBusConfig - constrcuts config object based on topic type(pub/sub),
// message bus type(tcp/ipc) and dev/prod mode
func GetMessageBusConfig(topic string, topicType string, devMode bool, cfgMgrConfig map[string]string) map[string]interface{} {
	var subTopics []string
	var topicConfigList []string
	appName := os.Getenv("AppName")
	cfgMgrCli := configmgr.Init("etcd", cfgMgrConfig)
	topic = strings.TrimSpace(topic)
	if strings.ToLower(topicType) == "sub" {
		subTopics = strings.Split(topic, "/")
		topic = subTopics[1]
	}

	if topicType == "server" || topicType == "client" {
		topicConfigList = strings.Split(os.Getenv("Server"), ",")
	} else {
		topicConfigList = strings.Split(os.Getenv(topic+"_cfg"), ",")
	}
	var messageBusConfig map[string]interface{}
	topicConfigList[0] = strings.TrimSpace(topicConfigList[0])
	topicConfigList[1] = strings.TrimSpace(topicConfigList[1])
	if topicConfigList[0] == "zmq_tcp" {
		address := strings.Split(topicConfigList[1], ":")
		hostname := address[0]
		port, err := strconv.ParseInt(address[1], 10, 64)
		if err != nil {
			glog.Errorf("string to int64 converstion Error: %v", err)
			os.Exit(1)
		}
		hostConfig := map[string]interface{}{
			"host": hostname,
			"port": port,
		}
		if strings.ToLower(topicType) == "pub" {
			messageBusConfig = map[string]interface{}{
				"type":            "zmq_tcp",
				"zmq_tcp_publish": hostConfig,
			}

			if !devMode {
				var allowedClients []string
				subscribers := strings.Split(os.Getenv("Clients"), ",")
				for _, subscriber := range subscribers {
					subscriber = strings.TrimSpace(subscriber)
					clientPublicKey, err := cfgMgrCli.GetConfig("/Publickeys/" + subscriber)
					if err != nil {
						glog.Errorf("Etcd GetConfig Error %v", err)
						os.Exit(1)
					}
					allowedClients = append(allowedClients, clientPublicKey)
				}
				serverSecretKey, err := cfgMgrCli.GetConfig("/" + appName + "/private_key")
				if err != nil {
					log.Fatal(err)
				}
				messageBusConfig["allowed_clients"] = allowedClients
				hostConfig["server_secret_key"] = serverSecretKey
			}
		} else if strings.ToLower(topicType) == "sub" {
			messageBusConfig = map[string]interface{}{
				"type": "zmq_tcp",
				topic:  hostConfig,
			}
			if !devMode {
				subTopics[0] = strings.TrimSpace(subTopics[0])
				serverPublicKey, err := cfgMgrCli.GetConfig("/Publickeys/" + subTopics[0])
				if err != nil {
					glog.Errorf("Etcd GetConfig Error %v", err)
					os.Exit(1)
				}
				clientSecretKey, err := cfgMgrCli.GetConfig("/" + appName + "/private_key")
				if err != nil {
					glog.Errorf("Etcd GetConfig Error %v", err)
					os.Exit(1)
				}
				clientPublicKey, err := cfgMgrCli.GetConfig("/Publickeys/" + appName)
				if err != nil {
					glog.Errorf("Etcd GetConfig Error %v", err)
					os.Exit(1)
				}
				hostConfig["server_public_key"] = serverPublicKey
				hostConfig["client_secret_key"] = clientSecretKey
				hostConfig["client_public_key"] = clientPublicKey
			}
		} else if strings.ToLower(topicType) == "server" {
			messageBusConfig = map[string]interface{}{
				"type": "zmq_tcp",
				topic:  hostConfig,
			}
			if !devMode {
				var allowedClients []string
				clients := strings.Split(os.Getenv("Clients"), ",")
				for _, client := range clients {
					client = strings.TrimSpace(client)
					clientPublicKey, err := cfgMgrCli.GetConfig("/Publickeys/" + client)
					if err != nil {
						glog.Errorf("Etcd GetConfig Error %v", err)
						os.Exit(1)
					}
					allowedClients = append(allowedClients, clientPublicKey)
				}
				serverSecretKey, err := cfgMgrCli.GetConfig("/" + appName + "/private_key")
				if err != nil {
					log.Fatal(err)
				}
				messageBusConfig["allowed_clients"] = allowedClients
				hostConfig["server_secret_key"] = serverSecretKey
			}
		} else if strings.ToLower(topicType) == "client" {
			messageBusConfig = map[string]interface{}{
				"type": "zmq_tcp",
				topic:  hostConfig,
			}
			if !devMode {
				clientPublicKey, err := cfgMgrCli.GetConfig("/Publickeys/" + appName)
				if err != nil {
					glog.Errorf("Etcd GetConfig Error %v", err)
					os.Exit(1)
				}

				clientSecretKey, err := cfgMgrCli.GetConfig("/" + appName + "/private_key")
				if err != nil {
					log.Fatal(err)
				}

				serverPublicKey, err := cfgMgrCli.GetConfig("/Publickeys/" + topic)
				if err != nil {
					glog.Errorf("Etcd GetConfig Error %v", err)
					os.Exit(1)
				}

				hostConfig["server_public_key"] = serverPublicKey
				hostConfig["client_secret_key"] = clientSecretKey
				hostConfig["client_public_key"] = clientPublicKey
			}
		} else {
			panic("Unsupported Topic Type!!!")
		}
	} else if topicConfigList[0] == "zmq_ipc" {
		messageBusConfig = map[string]interface{}{
			"type":       "zmq_ipc",
			"socket_dir": topicConfigList[1],
		}
	} else {
		panic("Unsupported MessageBus Type!!!")
	}
	return messageBusConfig
}

//GetTopics - returns list of topics based on topic type
func GetTopics(topicType string) []string {
	var topics []string
	if strings.ToLower(topicType) == "pub" {
		topics = strings.Split(os.Getenv("PubTopics"), ",")
	} else {
		topics = strings.Split(os.Getenv("SubTopics"), ",")
	}
	return topics
}
