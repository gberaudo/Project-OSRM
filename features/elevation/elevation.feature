@routing @elevation
Feature: Route with elevation

    Background: Use some profile
        Given the profile "car"


    Scenario: Route and retrieve elevation - match on elevations - short
        Given the node map
            | a |   |
            |   | b |

        And the ways
            | nodes |
            | ab    |

        And the nodes
            | node | ele        |
            | a    | 1732.18    |
            | b    | 98.45      |

        And elevation is on

        And compression is off

        When I route I should get
            | from | to | route | elevation       |
            | a    | b  | ab    | 1732.18 98.45   |




    Scenario: Route and retrieve elevation - match on elevations
        Given the node map
            | a |   |   | d |
            |   | b |   |   |
            |   |   | c |   |

        And the ways
            | nodes |
            | ab    |
            | bc    |
            | cd    |

        And the nodes
            | node | ele        |
            | a    | 1732.18    |
            | b    | 98.45      |
            | c    | -32.10     |
            | d    | 2.4321     |

        And elevation is on

        And compression is off

        When I route I should get
            | from | to | route    | elevation                  |
            | a    | c  | ab,bc    | 1732.18 98.45 -32.10       |
            | a    | d  | ab,bc,cd | 1732.18 98.45 -32.10 2.43  |
            | b    | c  | bc       | 98.45 -32.10               |
            | b    | d  | bc,cd    | 98.45 -32.10 2.43          |


    Scenario: Route and retrieve elevation - match on decoded geometry
        Given the node map
            | a |   |   | d |
            |   | b |   |   |
            |   |   | c |   |

        And the ways
            | nodes |
            | ab    |
            | bc    |
            | cd    |

        And the nodes
            | node | ele        |
            | a    | 1732.18    |
            | b    | 98.45      |
            | c    | -32.10     |
            | d    | 2.4321     |


        And elevation is on

        And compression is off

        When I route I should get
            | from | to | route | geometry                                                                     |
            | a    | c  | ab,bc | 1.000000,1.000000,1732.18 0.999100,1.000899,98.45 0.998201,1.001798,-32.10   |
            | b    | d  | bc,cd | 0.999100,1.000899,98.45 0.998201,1.001798,-32.10 1.000000,1.002697,2.43     |



    Scenario: Route and retrieve elevation - match encoded geometry
        Given the node map
            | a |   |   | d |
            |   | b |   |   |
            |   |   | c |   |

        And the ways
            | nodes |
            | ab    |
            | bc    |
            | cd    |

        And the nodes
            | node | ele        |
            | a    | 1732.18    |
            | b    | 98.45      |
            | c    | -32.10     |
            | d    | 2.4321     |


        And elevation is on

        When I route I should get
            | from | to | route | geometry                               |
            | a    | c  | ab,bc | _c`\|@_c`\|@ciqIfw@ew@xa~Hdw@ew@\|nX   |
            | b    | d  | bc,cd | wj~{@e{a\|@ifRdw@ew@\|nXmoBew@yvE      |

# polycodec.rb decode3 '_c`|@_c`|@ciqIfw@ew@xa~Hdw@ew@|nX'
# [[1.0, 1.0, 0.173218], [0.9991, 1.000899, 0.00984500000000002], [0.998201, 1.001798, -0.0032099999999999802]]

# polycodec.rb decode3 'wj~{@e{a|@ifRdw@ew@|nXmoBew@yvE'
# [[0.9991, 1.000899, 0.009845], [0.998201, 1.001798, -0.003210000000000001], [1.0, 1.002697, 0.00024299999999999886]]




@no_ele_data
    Scenario: Route with elevation without ele tags in osm data
        Given the node map
            | a |   |   | d |
            |   | b |   |   |
            |   |   | c |   |

        And the ways
            | nodes |
            | ab    |
            | bc    |
            | cd    |

        And elevation data is off

        And elevation is on

        And compression is off

        When I route I should get
            | from | to | route | geometry                                                                                    |
            | a    | c  | ab,bc | 1.000000,1.000000,21474836.47 0.999100,1.000899,21474836.47 0.998201,1.001798,21474836.47   |
            | b    | d  | bc,cd | 0.999100,1.000899,21474836.47 0.998201,1.001798,21474836.47 1.000000,1.002697,21474836.47   |



@no_elevation_request
    Scenario: Route without elevation
        Given the node map
            | a |   |   | d |
            |   | b |   |   |
            |   |   | c |   |

        And the ways
            | nodes |
            | ab    |
            | bc    |
            | cd    |

        When I route I should get
            | from | to | route | geometry                    |
            | a    | c  | ab,bc | _c`\|@_c`\|@fw@ew@dw@ew@  |
            | b    | d  | bc,cd | wj~{@e{a\|@dw@ew@moBew@   |
# mind the '\' before the pipe
# polycodec.rb decode3 '_c`|@_c`|@fw@ew@dw@ew@' [[1.0, 1.0, -0.0009], [1.000899, 0.999101, -1.0000000000000243e-06]]
# polycodec.rb decode3 'wj~{@e{a|@dw@ew@moBew@' [[0.9991, 1.000899, -0.000899], [0.999999, 1.002698, 0.0]]


