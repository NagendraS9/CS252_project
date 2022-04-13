--Athlete information
CREATE TABLE athletes (
    id INT,
    name TEXT,
    sex TEXT,
    age INT,
    weight TEXT,
    height TEXT
);

--Host cities information
CREATE TABLE host_cities (
    games TEXT,
    year INT,
    season TEXT,
    city TEXT
);

--Regions information
CREATE TABLE regions(
    noc TEXT,
    region TEXT,
    notes TEXT
);

--athlete events
CREATE TABLE   athlete_events (
    id INT,
    team TEXT,
    noc TEXT,
    games TEXT,
    sport TEXT,
    event TEXT,
    medal TEXT
);
