CREATE DATABASE baza_date;


USE baza_date;

CREATE TABLE Soferi (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nume VARCHAR(100) NOT NULL,
    strada VARCHAR(100),
    viteza INT DEFAULT 0,
    activ BOOLEAN DEFAULT 0,
    socket_descriptor INT
);


CREATE TABLE Strazi (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nume VARCHAR(100) NOT NULL,
    viteza_restrictie INT NOT NULL,
    vreme VARCHAR(100),
    evenimente_sportive VARCHAR(200),
    motorina_peco FLOAT,
    benzina_peco FLOAT
);

CREATE TABLE Optiuni (
    sofer_id INT,
    vreme TINYINT(1) DEFAULT 0,
    evenimente_sportive TINYINT(1) DEFAULT 0,
    preturi_peco TINYINT(1) DEFAULT 0,
    PRIMARY KEY (sofer_id),
    FOREIGN KEY (sofer_id) REFERENCES Soferi(id)
);

INSERT INTO Soferi (nume, strada, viteza, activ, socket_descriptor)
VALUES
('Popa Alexandru', 'Mamei', 20, 0, NULL),
('Baciu Matei', 'Lalelelor', 10, 0, NULL),
('Marin Cosmin', 'Păcii', 90, 0, NULL),
('Marin Vasile', 'Cuza', 50, 0, NULL),
('Miruna Miru', 'Eminescu', 30, 0, NULL);

INSERT INTO Strazi (nume, viteza_restrictie, vreme, evenimente_sportive, motorina_peco, benzina_peco)
VALUES
('Mamei', 50, 'soare', 'DINAMO-FCSB', 7, 6),
('Lalelelor', 50, 'ploaie', 'CONCERT-SMILEY', 7.50, 6.70),
('Păcii', 50, 'soare', NULL, 7, 6.10),
('Cuza', 80, 'ninsoare', 'BARCELONA-REAL_MADRID', 7.20, 6.95),
('Eminescu', 15, 'ceață', NULL, 7, 6.90);

INSERT INTO Optiuni (sofer_id, vreme, evenimente_sportive, preturi_peco)
VALUES
(1, 0, 0, 0),
(2, 0, 0, 0),
(3, 0, 0, 0),
(4, 0, 0, 0),
(5, 0, 0, 0);
